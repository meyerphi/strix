//! The GraalVM for interaction with the Owl library.

use std::ptr;

use crate::bindings::*;

/// An instance of the GraalVM.
pub struct Vm {
    /// The raw pointer to the isolate.
    isolate: *mut graal_isolate_t,
    /// The raw pointer to the current thread.
    pub(crate) thread: *mut graal_isolatethread_t,
}

impl Drop for Vm {
    fn drop(&mut self) {
        let result = unsafe { graal_detach_all_threads_and_tear_down_isolate(self.thread) };
        if result != 0 {
            panic!("Fatal error while dropping GraalVM: {}", result);
        }
    }
}

impl Vm {
    /// Creates a new instance of the GraalVM.
    ///
    /// # Errors
    ///
    /// Returns an error if the VM could not be intitialized.
    pub fn new() -> Result<Self, String> {
        let mut vm = Self {
            isolate: std::ptr::null_mut(),
            thread: std::ptr::null_mut(),
        };

        let result =
            unsafe { graal_create_isolate(ptr::null_mut(), &mut vm.isolate, &mut vm.thread) };
        if result == 0 {
            Ok(vm)
        } else {
            Err(format!("Fatal error while creating GraalVM: {}", result))
        }
    }
}
