use std::ptr;

use crate::bindings::*;

pub struct GraalVM {
    isolate: *mut graal_isolate_t,
    pub(crate) thread: *mut graal_isolatethread_t,
}

impl Drop for GraalVM {
    fn drop(&mut self) {
        let result = unsafe { graal_detach_all_threads_and_tear_down_isolate(self.thread) };
        if result != 0 {
            panic!("Fatal error while dropping GraalVM: {}", result);
        }
    }
}

impl GraalVM {
    pub fn new() -> Result<Self, String> {
        let mut vm = GraalVM {
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
