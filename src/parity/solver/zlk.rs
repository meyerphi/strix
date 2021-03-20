use crate::parity::game::{GameRegion, Parity, ParityGame, Player};
use crate::parity::solver::{ParityGameSolver, Strategy, WinningRegion};

use owl::Color;

struct ZlkSolverInstance<'a, G> {
    game: &'a G,
}

impl<'a, G: ParityGame<'a>> ZlkSolverInstance<'a, G> {
    fn new(game: &'a G) -> Self {
        ZlkSolverInstance { game }
    }

    fn largest_color(&self, disabled: &GameRegion) -> Option<Color> {
        (0..self.game.num_colors())
            .rev()
            .find(|&c| self.game.nodes_with_color(c).any(|i| !disabled[i]))
    }

    fn attractor(
        &self,
        disabled: &GameRegion,
        color: Color,
        parity: Parity,
        player: Player,
    ) -> GameRegion {
        let n = self.game.num_nodes();
        let mut a = GameRegion::with_capacity(n);
        let mut dis = disabled.clone();
        for c in (0..=color).rev() {
            let mut nodes = GameRegion::with_capacity(n);
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

    fn run(&self, disabled: &GameRegion) -> WinningRegion {
        match self.largest_color(disabled) {
            None => WinningRegion::with_capacity(self.game.num_nodes()),
            Some(color) => {
                let parity = Parity::of(color);
                let player = Player::from(parity);
                let a = self.attractor(disabled, color, parity, player);

                let disabled1 = disabled.union(&a);
                let mut won = self.run(&disabled1);
                let change = won[!player].attract_mut_without(self.game, !player, &disabled);
                if !change {
                    won[player].union_with(&a);
                } else {
                    let disabled2 = disabled.union(&won[!player]);
                    let won2 = self.run(&disabled2);
                    won[!player].union_with(&won2[!player]);
                    won[player] = won2.of(player);
                }
                won
            }
        }
    }
}

pub struct ZlkSolver {}

impl ZlkSolver {
    pub fn new() -> Self {
        ZlkSolver {}
    }
}

impl ParityGameSolver for ZlkSolver {
    fn solve<'a, G: ParityGame<'a>>(
        &mut self,
        game: &'a G,
        disabled: &GameRegion,
        player: Player,
        compute_strategy: bool,
    ) -> (GameRegion, Option<Strategy>) {
        // TODO add strategy computation
        assert!(!compute_strategy);
        let zlk = ZlkSolverInstance::new(game);
        let winning = zlk.run(&disabled);
        (winning.of(player), None)
    }
}
