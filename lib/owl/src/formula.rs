use std::convert::TryFrom;
use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_int, c_void};

use crate::bindings::*;
use crate::graal::GraalVM;

pub struct LTLFormula<'a> {
    vm: &'a GraalVM,
    pub(crate) formula: *mut c_void,
}

impl<'a> Drop for LTLFormula<'a> {
    fn drop(&mut self) {
        unsafe { destroy_object_handle(self.vm.thread, self.formula) };
    }
}

impl<'a> LTLFormula<'a> {
    pub fn parse<S: AsRef<str>>(vm: &'a GraalVM, formula: &str, propositions: &[S]) -> Self {
        let formula_c_string = CString::new(formula).unwrap();

        let p_cstring: Vec<_> = propositions
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();

        let p_ptr: Vec<_> = p_cstring
            .iter() // do NOT into_iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();

        let formula = unsafe {
            ltl_formula_parse(
                vm.thread,
                formula_c_string.as_ptr() as *mut _,
                p_ptr.as_ptr() as *mut *mut _,
                c_int::try_from(propositions.len()).unwrap(),
            )
        };
        LTLFormula { vm, formula }
    }

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

impl<'a> fmt::Display for LTLFormula<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut capacity = 256;
        let mut buffer = Vec::with_capacity(capacity);
        loop {
            buffer.resize(capacity, 0);
            let len = unsafe {
                print_object_handle(
                    self.vm.thread,
                    self.formula,
                    buffer.as_mut_ptr() as *mut i8,
                    buffer.len() as size_t,
                ) as usize
            };
            if len + 1 < capacity {
                buffer.truncate(len + 1);
                break;
            } else {
                capacity *= 2;
            }
        }
        let cstr = CStr::from_bytes_with_nul(&buffer).unwrap();
        write!(f, "{}", cstr.to_str().unwrap())?;
        Ok(())
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum AtomicPropositionStatus {
    True,
    False,
    Used,
    Unused,
}

impl AtomicPropositionStatus {
    fn from_c(status: atomic_proposition_status_t) -> Self {
        #![allow(non_upper_case_globals)]
        match status {
            atomic_proposition_status_t_CONSTANT_TRUE => AtomicPropositionStatus::True,
            atomic_proposition_status_t_CONSTANT_FALSE => AtomicPropositionStatus::False,
            atomic_proposition_status_t_USED => AtomicPropositionStatus::Used,
            atomic_proposition_status_t_UNUSED => AtomicPropositionStatus::Unused,
            _ => panic!("unsupported status: {}", status),
        }
    }
}
