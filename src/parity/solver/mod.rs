mod fpi;
mod incremental;
mod si;
mod zlk;

use std::fmt;
use std::ops::{Index, IndexMut};
use std::time::Duration;

use crate::parity::game::{GameRegion, NodeIndex, ParityGame, Player};
pub use fpi::FpiSolver;
pub use incremental::{IncrementalParityGameSolver, IncrementalSolver};
pub use si::SiSolver;
pub use zlk::ZlkSolver;

pub trait ParityGameSolver {
    fn solve<'a, G: ParityGame<'a>>(
        &mut self,
        game: &'a G,
        disabled: &GameRegion,
        player: Player,
        compute_strategy: bool,
    ) -> (GameRegion, Option<Strategy>);
}
#[derive(Debug, Clone)]
pub struct Strategy {
    data: Vec<Vec<NodeIndex>>,
}

impl Strategy {
    fn new() -> Self {
        Strategy { data: Vec::new() }
    }

    fn empty<'a, G: ParityGame<'a>>(game: &G) -> Self {
        Strategy {
            data: vec![Vec::new(); game.num_nodes()],
        }
    }

    fn grow(&mut self, n: usize) {
        if n > self.data.len() {
            self.data.resize(n, Vec::new());
        }
    }
}

impl Index<NodeIndex> for Strategy {
    type Output = Vec<NodeIndex>;

    fn index(&self, index: NodeIndex) -> &Self::Output {
        &self.data[index]
    }
}

impl IndexMut<NodeIndex> for Strategy {
    fn index_mut(&mut self, index: NodeIndex) -> &mut Self::Output {
        &mut self.data[index]
    }
}

#[derive(Debug, Clone)]
struct WinningRegion {
    even: GameRegion,
    odd: GameRegion,
}

impl WinningRegion {
    fn new() -> WinningRegion {
        WinningRegion {
            even: GameRegion::new(),
            odd: GameRegion::new(),
        }
    }

    fn with_capacity(n: usize) -> WinningRegion {
        WinningRegion {
            even: GameRegion::with_capacity(n),
            odd: GameRegion::with_capacity(n),
        }
    }

    fn of(self, player: Player) -> GameRegion {
        match player {
            Player::Even => self.even,
            Player::Odd => self.odd,
        }
    }
}

impl Index<Player> for WinningRegion {
    type Output = GameRegion;

    fn index(&self, index: Player) -> &Self::Output {
        match index {
            Player::Even => &self.even,
            Player::Odd => &self.odd,
        }
    }
}

impl IndexMut<Player> for WinningRegion {
    fn index_mut(&mut self, index: Player) -> &mut Self::Output {
        match index {
            Player::Even => &mut self.even,
            Player::Odd => &mut self.odd,
        }
    }
}

#[derive(Debug, Default, Clone)]
pub struct SolvingStats {
    nodes: usize,
    nodes_won_even: usize,
    nodes_won_odd: usize,
    time: Duration,
    time_inner_solver: Duration,
    time_strategy: Duration,
}

impl SolvingStats {
    pub fn nodes(&self) -> usize {
        self.nodes
    }

    pub fn nodes_won_even(&self) -> usize {
        self.nodes_won_even
    }

    pub fn nodes_won_odd(&self) -> usize {
        self.nodes_won_odd
    }

    pub fn time(&self) -> Duration {
        self.time
    }

    pub fn time_inner_solver(&self) -> Duration {
        self.time_inner_solver
    }

    pub fn time_strategy(&self) -> Duration {
        self.time_strategy
    }
}

impl fmt::Display for SolvingStats {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "|V+B|: {}, |W_even|: {}, |W_odd|: {}, solver time: {:.2}, inner solver time: {:.2}, strategy solver time: {:.2}",
            self.nodes(),
            self.nodes_won_even(),
            self.nodes_won_odd(),
            self.time().as_secs_f32(),
            self.time_inner_solver().as_secs_f32(),
            self.time_strategy().as_secs_f32(),
        )
    }
}
