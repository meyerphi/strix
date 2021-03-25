use crate::parity::game::{Game, Player, Region};
use crate::parity::solver::{ParityGameSolver, Strategy, WinningRegion};
use crate::parity::Parity;

use owl::automaton::Color;

struct ZlkSolverInstance<'a, G> {
    game: &'a G,
}

impl<'a, G: Game<'a>> ZlkSolverInstance<'a, G> {
    fn new(game: &'a G) -> Self {
        ZlkSolverInstance { game }
    }

    fn largest_color(&self, disabled: &Region) -> Option<Color> {
        (0..self.game.num_colors())
            .rev()
            .find(|&c| self.game.nodes_with_color(c).any(|i| !disabled[i]))
    }

    fn attractor(&self, disabled: &Region, color: Color, parity: Parity, player: Player) -> Region {
        let n = self.game.num_nodes();
        let mut a = Region::with_capacity(n);
        let mut dis = disabled.clone();
        for c in (0..=color).rev() {
            let mut nodes = Region::with_capacity(n);
            let mut empty = true;
            for i in self.game.nodes_with_color(c).filter(|&i| !disabled[i]) {
                nodes.insert(i);
                empty = false;
            }
            if !empty {
                if Parity::of(c) == parity {
                    nodes.attract_mut_without(self.game, player, &dis);
                    a.union_with(&nodes);
                    dis.union_with(&a);
                } else {
                    break;
                }
            }
        }
        a
    }

    fn run(&self, disabled: &Region) -> WinningRegion {
        match self.largest_color(disabled) {
            None => WinningRegion::with_capacity(self.game.num_nodes()),
            Some(color) => {
                let parity = Parity::of(color);
                let player = Player::from(parity);
                let a = self.attractor(disabled, color, parity, player);

                let disabled1 = disabled.union(&a);
                let mut won = self.run(&disabled1);
                let change = won[!player].attract_mut_without(self.game, !player, disabled);
                if change {
                    let disabled2 = disabled.union(&won[!player]);
                    let won2 = self.run(&disabled2);
                    won[!player].union_with(&won2[!player]);
                    won[player] = won2.of(player);
                } else {
                    won[player].union_with(&a);
                }
                won
            }
        }
    }
}

pub(crate) struct ZlkSolver {}

impl ZlkSolver {
    pub(crate) fn new() -> Self {
        Self {}
    }
}

impl ParityGameSolver for ZlkSolver {
    fn solve<'a, G: Game<'a>>(
        &mut self,
        game: &'a G,
        disabled: &Region,
        player: Player,
        compute_strategy: bool,
    ) -> (Region, Option<Strategy>) {
        // TODO add strategy computation
        assert!(!compute_strategy);
        let zlk = ZlkSolverInstance::new(game);
        let winning = zlk.run(disabled);
        (winning.of(player), None)
    }
}
