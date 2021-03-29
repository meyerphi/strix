//! Parity games and parity game solvers.

pub mod game;
pub(crate) mod solver;

use std::fmt;

use owl::automaton::Color;

/// A parity value: either even (0) or odd (1).
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Parity {
    /// Even parity (0).
    Even = 0,
    /// Odd parity (1).
    Odd = 1,
}

impl std::ops::Not for Parity {
    type Output = Self;

    fn not(self) -> Self::Output {
        match self {
            Self::Even => Self::Odd,
            Self::Odd => Self::Even,
        }
    }
}

impl fmt::Display for Parity {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        let string = match self {
            Self::Even => "even",
            Self::Odd => "odd",
        };
        write!(f, "{}", string)
    }
}

impl Parity {
    /// Returns the parity of the given color.
    pub fn of(color: Color) -> Self {
        match color % 2 {
            0 => Self::Even,
            1 => Self::Odd,
            _ => unreachable!(),
        }
    }
}

impl From<Parity> for Color {
    fn from(parity: Parity) -> Self {
        match parity {
            Parity::Even => 0,
            Parity::Odd => 1,
        }
    }
}
