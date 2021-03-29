//! Different types of controllers for a specification.

pub(crate) mod aiger;
pub(crate) mod bdd;
pub mod labelling;
pub(crate) mod machine;

pub use self::aiger::AigerController;
pub use bdd::BddController;
pub use machine::LabelledMachine;
