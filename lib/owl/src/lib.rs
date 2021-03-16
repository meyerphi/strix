pub mod automaton;
pub mod formula;
pub mod graal;
pub mod tree;

mod bindings;

use std::convert::TryFrom;

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct StateIndex(isize);

impl std::fmt::Display for StateIndex {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if self == &StateIndex::TOP {
            write!(f, "⊤")
        } else if self == &StateIndex::BOTTOM {
            write!(f, "⊥")
        } else {
            write!(f, "{}", self.0)
        }
    }
}

impl StateIndex {
    pub const TOP: StateIndex = StateIndex(-2);
    pub const BOTTOM: StateIndex = StateIndex(-1);

    fn try_from<I>(value: I) -> Result<Self, <isize as TryFrom<I>>::Error>
    where
        isize: TryFrom<I>,
    {
        Ok(Self(isize::try_from(value)?))
    }
}

pub type Color = usize;

#[derive(Copy, Clone, Debug)]
pub struct Edge<L> {
    successor: StateIndex,
    color: Color,
    label: L,
}

impl<L> Edge<L> {
    fn new(successor: StateIndex, color: Color, label: L) -> Edge<L> {
        Edge {
            successor,
            color,
            label,
        }
    }

    pub fn successor(&self) -> StateIndex {
        self.successor
    }

    pub fn color(&self) -> Color {
        self.color
    }

    pub fn label(&self) -> &L {
        &self.label
    }
}
