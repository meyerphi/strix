use std::cmp::Ordering;
use std::collections::VecDeque;

use tinyvec::TinyVec;

use owl::automaton::Color;

use crate::parity::game::{Game, Node, NodeIndex, Player, Region};
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
        val.extend(std::iter::repeat(0).take(num_colors));
        Self::Normal(val)
    }

    pub const fn is_finite(&self) -> bool {
        matches!(self, Self::Normal(_))
    }

    const fn upd(color: Color) -> i32 {
        1 - 2 * ((color % 2) as i32)
    }
}

impl std::fmt::Display for Valuation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NegativeInfinity => write!(f, "-∞"),
            Self::Infinity => write!(f, "∞"),
            Self::Normal(val) => {
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
            (Self::Normal(val), Self::Normal(other_val)) => {
                val.iter().rev().cmp(other_val.iter().rev())
            }
            (Self::Infinity, Self::Infinity) | (Self::NegativeInfinity, Self::NegativeInfinity) => {
                Ordering::Equal
            }
            (_, Self::Infinity) | (Self::NegativeInfinity, _) => Ordering::Less,
            (_, Self::NegativeInfinity) | (Self::Infinity, _) => Ordering::Greater,
        }
    }
}

impl std::ops::Add<Color> for Valuation {
    type Output = Self;

    fn add(mut self, rhs: Color) -> Self::Output {
        self += rhs;
        self
    }
}

impl std::ops::AddAssign<Color> for Valuation {
    fn add_assign(&mut self, rhs: Color) {
        if let Self::Normal(val) = self {
            val[rhs] += Self::upd(rhs);
        }
    }
}

impl std::ops::Sub<Color> for Valuation {
    type Output = Self;

    fn sub(mut self, rhs: Color) -> Self::Output {
        self -= rhs;
        self
    }
}

impl std::ops::SubAssign<Color> for Valuation {
    fn sub_assign(&mut self, rhs: Color) {
        if let Self::Normal(val) = self {
            val[rhs] -= Self::upd(rhs);
        }
    }
}

type GameValuation = Vec<Valuation>;
type GameValuationRef = [Valuation];

struct SiSolverInstance<'a, 'b, 'c, G: Game<'a>> {
    game: &'a G,
    disabled: &'b Region,
    strategy: &'c mut Strategy,
}

impl<'a, 'b, 'c, G: Game<'a>> SiSolverInstance<'a, 'b, 'c, G> {
    fn new(game: &'a G, disabled: &'b Region, initial_strategy: &'c mut Strategy) -> Self {
        initial_strategy.grow(game.num_nodes());
        SiSolverInstance {
            game,
            disabled,
            strategy: initial_strategy,
        }
    }

    fn run(mut self, player: Player) -> Region {
        let mut valuation;
        loop {
            valuation = self.bellman_ford(player);
            if !self.strategy_improvement(player, &valuation) {
                break;
            }
        }

        let mut winning = Region::with_capacity(self.game.num_nodes());
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

    fn evaluate_node(
        &self,
        player: Player,
        i: NodeIndex,
        valuation: &GameValuationRef,
    ) -> Valuation {
        fn minmax<I>(iter: I, min: bool, valuation: &GameValuationRef) -> Option<Valuation>
        where
            I: Iterator<Item = NodeIndex>,
        {
            let mapped = iter.map(|j| &valuation[j]);
            let minmax = if min { mapped.min() } else { mapped.max() };
            minmax.cloned()
        }

        let node = &self.game[i];
        let cur_player = Self::is_cur_player(node, player);
        let min = match player {
            Player::Even => false,
            Player::Odd => true,
        };
        let mut val = if cur_player {
            minmax(
                self.strategy[i]
                    .iter()
                    .filter(|&&j| !self.disabled[j])
                    .cloned(),
                min,
                valuation,
            )
            .unwrap_or_else(|| Valuation::zero(self.game.num_colors()))
        } else {
            minmax(
                node.successors()
                    .iter()
                    .cloned()
                    .filter(|&j| !self.disabled[j]),
                !min,
                valuation,
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
        let mut in_queue = Region::with_capacity(n);
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
        Self {
            strat_even: Strategy::new(),
            strat_odd: Strategy::new(),
        }
    }
}

impl ParityGameSolver for SiSolver {
    fn solve<'a, G: Game<'a>>(
        &mut self,
        game: &'a G,
        disabled: &Region,
        player: Player,
        compute_strategy: bool,
    ) -> (Region, Option<Strategy>) {
        let strategy = match player {
            Player::Even => &mut self.strat_even,
            Player::Odd => &mut self.strat_odd,
        };
        let solver = SiSolverInstance::new(game, disabled, strategy);
        let winning = solver.run(player);
        (winning, compute_strategy.then(|| strategy.clone()))
    }
}
