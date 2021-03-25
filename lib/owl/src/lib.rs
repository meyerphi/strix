//! Bindings to the Owl library for Omega-words, ω-automata and Linear Temporal Logic (LTL).
//!
//! All entry points to the Owl library first require an instance of the Graal VM in [`graal::Vm`].
//! Afterwards, LTL formulas can be parsed by [`formula::Ltl`] and automata can be created by [`automaton::Automaton`].
//!
//! # Examples
//!
//! A max-even DPA for the LTL formula "G (r -> F g)" can be created and queried as follows:
//! ```
//! # use owl::{graal, formula, automaton};
//! use automaton::MaxEvenDpa;
//!
//! let vm = graal::Vm::new().unwrap();
//! let ltl = formula::Ltl::parse(&vm, "G (r -> F g)", &["r", "g"]);
//! let mut automaton = automaton::Automaton::of(&vm, &ltl, true);
//! let q0 = automaton.initial_state();
//! let edges = automaton.successors(q0);
//!
//! // successor with "r" and "g"
//! let edge0 = edges.lookup(&[true, true]);
//! // successor with "r" and not "g"
//! let edge1 = edges.lookup(&[true, false]);
//!
//! assert_eq!(edge0.successor(), q0);
//! assert_ne!(edge1.successor(), q0);
//! assert_eq!(edge0.color() % 2, 0);
//! assert_eq!(edge1.color() % 2, 1);
//! ```

#[doc(hidden)]
mod bindings;

pub mod automaton;
pub mod formula;
pub mod graal;
pub mod tree;
