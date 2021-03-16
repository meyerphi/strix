use std::cmp::Ordering;
use std::collections::VecDeque;

use tinyvec::TinyVec;

use owl::Color;

use crate::parity::game::{GameRegion, NodeIndex, ParityGame, ParityNode, Player};
use crate::parity::solver::{ParityGameSolver, Strategy};

#[derive(Debug, Clone, PartialEq, Eq)]
enum Valuation {
    Normal(TinyVec<[i32; 16]>),
    Infinity,
    NegativeInfinity,
}

impl Valuation {
    pub fn zero(num_colors: usize) -> Self {
        let mut val = TinyVec::<[i32; 16]>::with_capacity(num_colors);
        for _ in 0..num_colors {
            val.push(0);
        }
        Valuation::Normal(val)
    }

    pub fn is_finite(&self) -> bool {
        matches!(self, Valuation::Normal(_))
    }

    fn upd(color: Color) -> i32 {
        1 - 2 * ((color % 2) as i32)
    }
}

impl std::fmt::Display for Valuation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Valuation::NegativeInfinity => write!(f, "-∞"),
            Valuation::Infinity => write!(f, "∞"),
            Valuation::Normal(val) => {
                write!(f, "[ ")?;
                for &v in val {
                    write!(f, " {}", v)?;
                }
                write!(f, " ]")?;
                Ok(())
            }
        }
    }
}

impl PartialOrd for Valuation {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Valuation {
    fn cmp(&self, other: &Self) -> Ordering {
        match (self, other) {
            (Valuation::Normal(val), Valuation::Normal(other_val)) => {
                val.iter().rev().cmp(other_val.iter().rev())
            }
            (Valuation::Infinity, Valuation::Infinity)
            | (Valuation::NegativeInfinity, Valuation::NegativeInfinity) => Ordering::Equal,
            (_, Valuation::Infinity) | (Valuation::NegativeInfinity, _) => Ordering::Less,
            (_, Valuation::NegativeInfinity) | (Valuation::Infinity, _) => Ordering::Greater,
        }
    }
}

impl std::ops::Add<Color> for Valuation {
    type Output = Valuation;

    fn add(mut self, rhs: Color) -> Self::Output {
        self += rhs;
        self
    }
}

impl std::ops::AddAssign<Color> for Valuation {
    fn add_assign(&mut self, rhs: Color) {
        if let Valuation::Normal(val) = self {
            val[rhs] += Self::upd(rhs);
        }
    }
}

impl std::ops::Sub<Color> for Valuation {
    type Output = Valuation;

    fn sub(mut self, rhs: Color) -> Self::Output {
        self -= rhs;
        self
    }
}

impl std::ops::SubAssign<Color> for Valuation {
    fn sub_assign(&mut self, rhs: Color) {
        if let Valuation::Normal(val) = self {
            val[rhs] -= Self::upd(rhs);
        }
    }
}

type GameValuation = Vec<Valuation>;
type GameValuationRef = [Valuation];

struct SiSolverInstance<'a, 'b, 'c, G: ParityGame<'a>> {
    game: &'a G,
    disabled: &'b GameRegion,
    strategy: &'c mut Strategy,
}

impl<'a, 'b, 'c, G: ParityGame<'a>> SiSolverInstance<'a, 'b, 'c, G> {
    fn new(game: &'a G, disabled: &'b GameRegion, initial_strategy: &'c mut Strategy) -> Self {
        initial_strategy.grow(game.num_nodes());
        SiSolverInstance {
            game,
            disabled,
            strategy: initial_strategy,
        }
    }

    fn run(mut self, player: Player) -> GameRegion {
        let mut valuation;
        loop {
            valuation = self.bellman_ford(player);
            if !self.strategy_improvement(player, &valuation) {
                break;
            }
        }

        let mut winning = GameRegion::with_capacity(self.game.num_nodes());
        // obtain winning region and set correct strategy for winning nodes
        for i in self.game.nodes() {
            if !self.disabled[i] && !valuation[i].is_finite() {
                winning.insert(i);
                self.strategy[i].retain(|&j| !valuation[j].is_finite());
            }
        }
        winning
    }

    fn strategy_improvement(&mut self, player: Player, valuation: &GameValuationRef) -> bool {
        let goal = Self::player_goal(player);
        let mut change = false;
        for i in self.game.nodes() {
            let node = &self.game[i];
            if !self.disabled[i] && Self::is_cur_player(node, player) && valuation[i].is_finite() {
                let val_cmp = valuation[i].clone() - node.color();

                self.strategy[i].clear();
                for &j in self.game[i].successors() {
                    if !self.disabled[j] {
                        let cmp = valuation[j].cmp(&val_cmp);
                        if cmp == goal || cmp == Ordering::Equal {
                            // improvement
                            self.strategy[i].push(j);
                        }
                        if cmp == goal {
                            // strict improvement
                            change = true;
                        }
                    }
                }
            }
        }
        change
    }

    fn is_cur_player(node: &'a G::Node, player: Player) -> bool {
        node.owner() == player || node.successors().len() == 1
    }

    fn player_goal(player: Player) -> Ordering {
        match player {
            Player::Even => Ordering::Greater,
            Player::Odd => Ordering::Less,
        }
    }

    fn init_node(player: Player) -> Valuation {
        match player {
            Player::Even => Valuation::Infinity,
            Player::Odd => Valuation::NegativeInfinity,
        }
    }

    fn minmax<I>(&self, iter: I, min: bool, valuation: &GameValuationRef) -> Option<Valuation>
    where
        I: Iterator<Item = NodeIndex>,
    {
        let mapped = iter.map(|j| &valuation[j]);
        let minmax = if min { mapped.min() } else { mapped.max() };
        minmax.cloned()
    }

    fn evaluate_node(
        &self,
        player: Player,
        i: NodeIndex,
        valuation: &GameValuationRef,
    ) -> Valuation {
        let node = &self.game[i];
        let cur_player = Self::is_cur_player(node, player);
        let min = match player {
            Player::Even => false,
            Player::Odd => true,
        };
        let mut val = if cur_player {
            self.minmax(
                self.strategy[i]
                    .iter()
                    .filter(|&&j| !self.disabled[j])
                    .cloned(),
                min,
                &valuation,
            )
            .unwrap_or_else(|| Valuation::zero(self.game.num_colors()))
        } else {
            self.minmax(
                node.successors()
                    .iter()
                    .cloned()
                    .filter(|&j| !self.disabled[j]),
                !min,
                &valuation,
            )
            .unwrap()
        };
        val += node.color();
        val
    }

    fn bellman_ford(&mut self, player: Player) -> GameValuation {
        let n = self.game.num_nodes();
        let mut valuation = vec![Self::init_node(player); n];

        let mut queue = VecDeque::with_capacity(n);
        let mut in_queue = GameRegion::with_capacity(n);
        for i in self.game.nodes() {
            if !self.disabled[i]
                && Self::is_cur_player(&self.game[i], player)
                && self.strategy[i].iter().all(|&j| self.disabled[j])
            {
                queue.push_back(i);
                in_queue.set(i, true);
            }
        }
        while let Some(i) = queue.pop_front() {
            in_queue.set(i, false);
            let val = self.evaluate_node(player, i, &valuation);
            if val != valuation[i] {
                valuation[i] = val;
                for &j in self.game[i].predecessors() {
                    if !self.disabled[j] && !in_queue[j] {
                        queue.push_back(j);
                        in_queue.set(j, true);
                    }
                }
            }
        }
        valuation
    }
}

pub struct SiSolver {
    strat_even: Strategy,
    strat_odd: Strategy,
}

impl SiSolver {
    pub fn new() -> Self {
        SiSolver {
            strat_even: Strategy::new(),
            strat_odd: Strategy::new(),
        }
    }
}

impl ParityGameSolver for SiSolver {
    fn solve<'a, G: ParityGame<'a>>(
        &mut self,
        game: &'a G,
        disabled: &GameRegion,
        player: Player,
        compute_strategy: bool,
    ) -> (GameRegion, Option<Strategy>) {
        let strategy = match player {
            Player::Even => &mut self.strat_even,
            Player::Odd => &mut self.strat_odd,
        };
        let solver = SiSolverInstance::new(game, disabled, strategy);
        let winning = solver.run(player);
        (winning, compute_strategy.then(|| strategy.clone()))
    }
}
