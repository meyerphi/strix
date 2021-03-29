//! LTL formulas.

use std::convert::TryFrom;
use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_int, c_void};

use crate::bindings::*;
use crate::graal::Vm;

/// An LTL formula object from Owl.
pub struct Ltl<'a> {
    /// The used GraalVM.
    pub(crate) vm: &'a Vm,
    /// The raw pointer to the formula object.
    pub(crate) formula: *mut c_void,
}

impl<'a> Drop for Ltl<'a> {
    fn drop(&mut self) {
        unsafe { destroy_object_handle(self.vm.thread, self.formula) };
    }
}

impl<'a> Ltl<'a> {
    /// Parses the given formula with the list of atomic propositions.
    pub fn parse<S: AsRef<str>>(vm: &'a Vm, formula: &str, propositions: &[S]) -> Self {
        let formula_c_string = CString::new(formula).unwrap();

        let p_cstring: Vec<_> = propositions
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();

        let p_ptr: Vec<_> = p_cstring
            .iter() // do NOT into_iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();

        let formula_ptr = unsafe {
            ltl_formula_parse(
                vm.thread,
                formula_c_string.as_ptr() as *mut _,
                p_ptr.as_ptr() as *mut *mut _,
                c_int::try_from(propositions.len()).unwrap(),
            )
        };
        Ltl {
            vm,
            formula: formula_ptr,
        }
    }

    /// Simplifies the formula with the realizability simplifier,
    /// wher the atomic propositions with index `0..num_inputs` are considered
    /// inputs and the atomic propositions with index `num_inputs..(num_inputs + num_outputs)`
    /// are considered outputs.
    /// Returns the status for each proposition after simplification.
    pub fn simplify(
        &mut self,
        num_inputs: usize,
        num_outputs: usize,
    ) -> Vec<AtomicPropositionStatus> {
        let num_vars = num_inputs + num_outputs;
        let mut owl_statuses: Vec<c_int> = vec![0; num_vars];

        self.formula = unsafe {
            ltl_formula_simplify(
                self.vm.thread,
                self.formula,
                num_inputs as c_int,
                owl_statuses.as_mut_ptr(),
                num_vars as c_int,
            )
        };

        owl_statuses
            .into_iter()
            .map(|s| AtomicPropositionStatus::from_c(s as atomic_proposition_status_t))
            .collect()
    }
}

impl<'a> fmt::Display for Ltl<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut capacity = 256;
        let mut buffer = vec![0; capacity];
        loop {
            let len = unsafe {
                print_object_handle(
                    self.vm.thread,
                    self.formula,
                    buffer.as_mut_ptr() as *mut i8,
                    buffer.len() as size_t,
                ) as usize
            };
            if len + 1 < capacity {
                // whole object could be printed to buffer
                buffer.truncate(len + 1);
                let cstr = CStr::from_bytes_with_nul(&buffer).unwrap();
                write!(f, "{}", cstr.to_str().unwrap())?;
                return Ok(());
            }
            // need to increase capacity and repeat
            capacity *= 2;
            buffer.resize(capacity, 0);
        }
    }
}

/// The status of an atomic proposition after realizability simplification.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum AtomicPropositionStatus {
    /// The proposition only occurs positively and has been replaced with true.
    True,
    /// The proposition only occurs negatively and has been replaced with false.
    False,
    /// The proposition occurs both positively and negatively in the formula.
    Used,
    /// The proposition does not occur in the formula.
    Unused,
}

impl AtomicPropositionStatus {
    /// Converts the atomic proposition status from Owl.
    fn from_c(status: atomic_proposition_status_t) -> Self {
        #![allow(non_upper_case_globals)]
        match status {
            atomic_proposition_status_t_CONSTANT_TRUE => Self::True,
            atomic_proposition_status_t_CONSTANT_FALSE => Self::False,
            atomic_proposition_status_t_USED => Self::Used,
            atomic_proposition_status_t_UNUSED => Self::Unused,
            _ => panic!("unsupported status: {}", status),
        }
    }
}
