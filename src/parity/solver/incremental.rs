use std::time::Instant;

use crate::parity::game::{ParityGame, Player};
use crate::parity::solver::{ParityGameSolver, SolvingStats, Strategy, WinningRegion};

pub trait IncrementalParityGameSolver {
    fn solve<'a, G: ParityGame<'a>>(&mut self, game: &'a G) -> Option<Player>;
    fn strategy<'a, G: ParityGame<'a>>(&mut self, game: &'a G, player: Player) -> Strategy;
}

pub struct IncrementalSolver<S: ParityGameSolver> {
    winning: WinningRegion,
    solver: S,
    stats: SolvingStats,
}

impl<S: ParityGameSolver> IncrementalSolver<S> {
    pub fn new(solver: S) -> Self {
        IncrementalSolver {
            winning: WinningRegion::new(),
            solver,
            stats: SolvingStats::default(),
        }
    }
}

impl<S: ParityGameSolver> IncrementalParityGameSolver for IncrementalSolver<S> {
    fn solve<'a, G: ParityGame<'a>>(&mut self, game: &'a G) -> Option<Player> {
        let start = Instant::now();

        let n = game.num_nodes();

        for &player in &Player::PLAYERS {
            // extend winning region with attractor
            self.winning[player].grow(n);
            self.winning[player].attract_mut(game, player);
        }
        for &player in &Player::PLAYERS {
            // Remove corresponding border attractor and already won nodes
            let mut disabled = self.winning[!player].union(game.border());
            disabled.attract_mut_without(game, &self.winning[player], !player);
            disabled.attract_mut(game, !player);
            disabled.union_with(&self.winning[player]);

            let start_inner = Instant::now();
            let (winning_new, _) = self.solver.solve(game, &disabled, player, false);
            self.stats.time_inner_solver += start_inner.elapsed();

            // add new winning region to existing region
            self.winning[player].union_with(&winning_new);
        }
        self.stats.nodes = n;
        self.stats.time += start.elapsed();
        self.stats.nodes_won_even = self.winning[Player::Even].size();
        self.stats.nodes_won_odd = self.winning[Player::Odd].size();

        // Get winner of initial node
        let node = game.initial_node();
        if self.winning[Player::Even][node] {
            Some(Player::Even)
        } else if self.winning[Player::Odd][node] {
            Some(Player::Odd)
        } else {
            None
        }
    }

    fn strategy<'a, G: ParityGame<'a>>(&mut self, game: &'a G, player: Player) -> Strategy {
        let start = Instant::now();

        let border = game.border().attract(game, !player);
        let (_, strategy) = self.solver.solve(game, &border, player, true);

        self.stats.time_strategy += start.elapsed();
        strategy.expect("no winning strategy")
    }
}

impl<S: ParityGameSolver> IncrementalSolver<S> {
    pub fn stats(&self) -> &SolvingStats {
        &self.stats
    }
}
