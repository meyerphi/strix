use std::fmt;
use std::io::{self, Write};

use abc::Abc;
use aiger::{Aiger, AigerMode};
use log::{info, trace};

/// A controller as an and-inverter-graph / aiger circuit.
pub struct AigerController {
    aig: Aiger,
}

impl AigerController {
    pub(super) fn new(aig: Aiger) -> Self {
        Self { aig }
    }

    /// Writes the aiger controller to the given writer. The controller
    /// is written in binary mode if the binary flag is true, and otherwise
    /// in ASCII mode.
    ///
    /// # Errors
    ///
    /// Returns an error if an I/O error occurs during writing.
    pub fn write<W: Write>(&self, writer: W, binary: bool) -> io::Result<()> {
        self.aig.write(
            writer,
            if binary {
                AigerMode::Binary
            } else {
                AigerMode::Ascii
            },
        )
    }

    fn execute_compress_commands(abc: &mut Abc, all_methods: bool) {
        abc.balance(false, false);
        abc.resubstitute(8, 1);
        abc.rewrite(false, false);
        abc.resubstitute(6, 2);
        abc.refactor(10, 16, false, false);
        abc.resubstitute(8, 1);
        abc.balance(false, false);
        abc.resubstitute(8, 2);
        abc.rewrite(false, false);
        abc.resubstitute(10, 1);
        abc.rewrite(true, false);
        abc.resubstitute(10, 2);
        abc.balance(false, false);
        abc.resubstitute(12, 1);
        abc.refactor(10, 16, false, false);
        abc.resubstitute(12, 2);
        abc.balance(false, false);
        abc.rewrite(true, false);
        abc.balance(false, false);
        if all_methods {
            abc.drewrite(8, 5, false, true);
            abc.drefactor(2, 12, 5, false, false);
            abc.balance(false, false);
            abc.drewrite(8, 5, false, true);
            abc.drewrite(8, 5, true, true);
            abc.balance(false, false);
            abc.drefactor(2, 12, 5, false, true);
            abc.balance(false, false);
        }
    }

    pub(crate) fn compress(&mut self, all_methods: bool) {
        info!("Compressing aiger circuit of size {}", self.size());

        let mut abc = Abc::new().unwrap();
        abc.set_aiger(&self.aig);
        abc.zero();
        let mut size = abc.network_size();
        let mut old_size = size + 1;
        while size > 0 && size < old_size {
            Self::execute_compress_commands(&mut abc, all_methods);
            old_size = size;
            size = abc.network_size();
            trace!("Compression size now at {}", size);
        }
        let aig = abc.get_aiger();
        self.aig = aig;
        info!("Compressed aiger circuit has size {}", self.size());
    }

    pub(crate) fn size(&self) -> AigerSize {
        AigerSize {
            num_ands: self.aig.num_ands() as u32,
            num_latches: self.aig.num_latches() as u32,
        }
    }
}

impl fmt::Display for AigerController {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.aig)
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct AigerSize {
    num_ands: u32,
    num_latches: u32,
}

impl AigerSize {
    pub(crate) fn total(&self) -> u32 {
        self.num_ands + self.num_latches
    }
}

impl std::ops::Mul<u32> for AigerSize {
    type Output = Self;

    fn mul(self, rhs: u32) -> Self::Output {
        Self {
            num_ands: self.num_ands * rhs,
            num_latches: self.num_latches * rhs,
        }
    }
}

impl fmt::Display for AigerSize {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "(#ands: {}, #latches: {})",
            self.num_ands, self.num_latches
        )
    }
}
