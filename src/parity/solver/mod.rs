mod fpi;
mod incremental;
mod si;
mod zlk;

use std::fmt;
use std::ops::{Index, IndexMut};
use std::time::Duration;

use crate::parity::game::{Game, NodeIndex, Player, Region};
pub(crate) use fpi::FpiSolver;
pub(crate) use incremental::{IncrementalParityGameSolver, IncrementalSolver};
pub(crate) use si::SiSolver;
pub(crate) use zlk::ZlkSolver;

pub trait ParityGameSolver {
    fn solve<'a, G: Game<'a>>(
        &mut self,
        game: &'a G,
        disabled: &Region,
        player: Player,
        compute_strategy: bool,
    ) -> (Region, Option<Strategy>);
}
#[derive(Debug, Clone)]
pub struct Strategy {
    data: Vec<Vec<NodeIndex>>,
}

impl Strategy {
    fn new() -> Self {
        Self { data: Vec::new() }
    }

    fn empty<'a, G: Game<'a>>(game: &G) -> Self {
        Self {
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
    even: Region,
    odd: Region,
}

impl WinningRegion {
    fn new() -> Self {
        Self {
            even: Region::new(),
            odd: Region::new(),
        }
    }

    fn with_capacity(n: usize) -> Self {
        Self {
            even: Region::with_capacity(n),
            odd: Region::with_capacity(n),
        }
    }

    fn of(self, player: Player) -> Region {
        match player {
            Player::Even => self.even,
            Player::Odd => self.odd,
        }
    }
}

impl Index<Player> for WinningRegion {
    type Output = Region;

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
