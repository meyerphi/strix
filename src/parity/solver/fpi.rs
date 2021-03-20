/*
 * This file has been modified by Philipp Meyer from the original
 * file fpi.cpp in Oink available at: https://github.com/trolando/oink
 *
 * Copyright 2017-2018 Tom van Dijk, Johannes Kepler University Linz
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use owl::Color;

use crate::parity::game::{Game, Node, NodeIndex, Parity, Player, Region};
use crate::parity::solver::{ParityGameSolver, Strategy};

struct FpiSolverInstance<'a, 'b, G> {
    game: &'a G,
    disabled: &'b Region,
    frozen: Vec<Color>,
    distraction: Vec<bool>,
}

impl<'a, 'b, G: Game<'a>> FpiSolverInstance<'a, 'b, G> {
    fn new(game: &'a G, disabled: &'b Region) -> Self {
        Self {
            game,
            disabled,
            frozen: vec![0; game.num_nodes()],
            distraction: vec![false; game.num_nodes()],
        }
    }

    fn winner(&self, i: NodeIndex) -> Player {
        let player = Player::from(self.game[i].parity());
        if self.distraction[i] {
            !player
        } else {
            player
        }
    }

    fn update_block(&mut self, strategy: Option<&mut Strategy>, player: Player, c: Color) -> bool {
        let mut unchanged = true;
        for i in self.game.nodes_with_color(c) {
            if self.disabled[i] || self.frozen[i] != 0 || self.distraction[i] {
                continue;
            }
            let node = &self.game[i];

            // Determine one-step winner
            let owner = node.owner();
            let mut good_successors = node
                .successors()
                .iter()
                .filter(|&&j| !self.disabled[j] && owner == self.winner(j))
                .peekable();
            let onestep_winner = if good_successors.peek().is_some() {
                owner
            } else {
                !owner
            };
            // Update strategy
            if let Some(&mut ref mut strategy) = strategy {
                if player == onestep_winner {
                    strategy[i].clear();
                    strategy[i].extend(good_successors);
                }
            }
            // Update distraction if estimate of winner changed
            if onestep_winner != self.winner(i) {
                self.distraction[i] = true;
                unchanged = false;
            }
        }
        unchanged
    }

    fn freeze_thaw_reset(&mut self, c: Color) {
        let p = Parity::of(c);
        for b in 0..c {
            for i in self.game.nodes_with_color(b) {
                if self.disabled[i] || self.frozen[i] >= c {
                    continue;
                }
                let parity = self.game[i].parity();
                let frozen = &mut self.frozen[i];
                let distraction = &mut self.distraction[i];

                if *frozen != 0 {
                    if Parity::of(*frozen) == p {
                        *frozen = c;
                    } else {
                        *frozen = 0;
                        *distraction = false;
                    }
                } else if *distraction {
                    if parity == p {
                        *frozen = c;
                    } else {
                        *distraction = false;
                    }
                } else if parity != p {
                    *frozen = c;
                }
            }
        }
    }

    fn run(&mut self, player: Player, compute_strategy: bool) -> (Region, Option<Strategy>) {
        let mut strategy = compute_strategy.then(|| Strategy::empty(self.game));

        // Main loop
        let mut c = 0;
        while c < self.game.num_colors() {
            if self.update_block(strategy.as_mut(), player, c) {
                c += 1;
            } else {
                self.freeze_thaw_reset(c);
                c = 0;
            }
        }

        // Construct winning region
        let mut winning_region = Region::with_capacity(self.game.num_nodes());
        winning_region.extend(
            self.game
                .nodes()
                .filter(|&i| !self.disabled[i] && self.winner(i) == player),
        );

        (winning_region, strategy)
    }
}

pub struct FpiSolver {}

impl FpiSolver {
    pub fn new() -> Self {
        Self {}
    }
}

impl ParityGameSolver for FpiSolver {
    fn solve<'a, G: Game<'a>>(
        &mut self,
        game: &'a G,
        disabled: &Region,
        player: Player,
        compute_strategy: bool,
    ) -> (Region, Option<Strategy>) {
        FpiSolverInstance::new(game, disabled).run(player, compute_strategy)
    }
}
