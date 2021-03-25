//! Bindings to the ABC library with a selective set of functions
//! for rewriting aiger circuits.

#[doc(hidden)]
mod bindings;

use std::error::Error;
use std::fmt;
use std::os::raw::c_int;

use ::aiger::Aiger;

use bindings::*;

/// An instance of the ABC framework.
#[derive(Debug)]
pub struct Abc {
    /// Raw pointer to the frame.
    frame: *mut Abc_Frame_t,
}

impl Drop for Abc {
    fn drop(&mut self) {
        unsafe { Abc_Stop(self.frame) }
    }
}

/// An error returned by the ABC framework.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum AbcError {
    /// The ABC framework could not perform an operation because memory could not be allocated.
    MemoryOut,
}

impl fmt::Display for AbcError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "ABC error: {}",
            match self {
                Self::MemoryOut => "Out of memory",
            }
        )
    }
}

impl Error for AbcError {}

impl Abc {
    /// Creates a new instance of the ABC framework.
    ///
    /// # Errors
    ///
    /// Returns an error if the framework can not be initialized.
    pub fn new() -> Result<Self, AbcError> {
        let frame = unsafe { Abc_Start() };
        if frame.is_null() {
            Err(AbcError::MemoryOut)
        } else {
            Ok(Self { frame })
        }
    }

    /// Loads the given aiger circuit and sets it as the current network.
    pub fn set_aiger(&mut self, aiger: &Aiger) {
        let aiger_ptr = unsafe { aiger.raw_ptr() } as *mut bindings::aiger;
        let ntk = unsafe { Io_LoadAiger(aiger_ptr, true as c_int) };
        unsafe { Abc_FrameReplaceNetwork(self.frame, ntk) };
    }
    /// Reads the current network and returns it as an aiger circuit.
    pub fn get_aiger(&mut self) -> Aiger {
        let ntk = unsafe { Abc_FrameReadNtk(self.frame) };
        let aiger_ptr = unsafe { Io_StoreAiger(ntk, true as c_int) };
        unsafe { Aiger::from_raw(aiger_ptr as *mut ::aiger::AigerRaw) }
    }

    /// Returns the raw pointer to the current network.
    fn get_network(&self) -> *mut Abc_Ntk_t {
        unsafe { Abc_FrameReadNtk(self.frame) }
    }
    /// Replaces the current network with given network.
    fn set_network(&mut self, ntk: *mut Abc_Ntk_t) {
        unsafe { Abc_FrameReplaceNetwork(self.frame, ntk) };
    }

    /// Applies the given function to the current network.
    ///
    /// # Panics
    ///
    /// Panics if the function returns a zero exit code.
    fn change_network<F>(&mut self, f: F)
    where
        F: FnOnce(*mut Abc_Ntk_t) -> c_int,
    {
        let ntk = self.get_network();
        let result = f(ntk);
        assert!(result != 0);
    }
    /// Applies the given function to the current network
    /// and replaces it with the result of the function.
    fn change_network_with<F>(&mut self, f: F)
    where
        F: FnOnce(*mut Abc_Ntk_t) -> *mut Abc_Ntk_t,
    {
        let ntk = self.get_network();
        let ntk_new = f(ntk);
        self.set_network(ntk_new);
    }

    /// Returns the size of the current network as the number of nodes.
    pub fn network_size(&self) -> usize {
        let ntk = unsafe { Abc_FrameReadNtk(self.frame) };
        let nodes = unsafe { Abc_NtkNetworkSize(ntk) };
        nodes as usize
    }

    /// Convert all latches in the current network to have a constant zero as initial value.
    pub fn zero(&mut self) {
        self.change_network_with(|ntk| unsafe { Abc_NtkRestrashZero(ntk) });
    }
    /// Transforms the current network into a well-balanced AIG.
    ///
    /// # Arguments
    ///
    /// * `duplicative`: Perform duplication of logic (default: `false`).
    /// * `selective`: Perform duplication on the critical paths (default: `false`).
    pub fn balance(&mut self, duplicative: bool, selective: bool) {
        self.change_network_with(|ntk| unsafe {
            Abc_NtkBalance(ntk, duplicative as c_int, selective as c_int)
        });
    }
    /// Performs technology-independent restructuring of the AIG.
    ///
    /// # Arguments
    ///
    /// * `cuts_max`: The maximum cut size. Must be in range `4..=16` (default: `8`).
    /// * `nodes_max`: The maximum number of nodes to add. Must be in range `0..=3` (default: `1`).
    ///
    /// # Panics
    ///
    /// Panics if an argument is out of range.
    pub fn resubstitute(&mut self, cuts_max: usize, nodes_max: usize) {
        assert!((4..=16).contains(&cuts_max));
        assert!((0..=3).contains(&nodes_max));
        self.change_network(|ntk| unsafe {
            Abc_NtkResubstitute(ntk, cuts_max as c_int, nodes_max as c_int)
        });
    }
    /// Performs technology-independent refactoring of the AIG.
    ///
    /// # Arguments
    ///
    /// * `node_size_max`: The maximum support of the collapsed nodes. Must be in range `0..=15` (default: `10`).
    /// * `cone_size_max`: The maximum support of the containing cone. Must be greater than `node_size_max` if don't cares are used (default: `16`).
    /// * `use_zeros`: Use zero-cost replacements (default: `false`).
    /// * `use_dcs`: Use don't cares (default: `false`).
    ///
    /// # Panics
    ///
    /// Panics if an argument is out of range.
    pub fn refactor(
        &mut self,
        node_size_max: usize,
        cone_size_max: usize,
        use_zeros: bool,
        use_dcs: bool,
    ) {
        assert!(node_size_max <= 15);
        assert!(!use_dcs || node_size_max < cone_size_max);
        self.change_network(|ntk| unsafe {
            Abc_NtkRefactor(
                ntk,
                node_size_max as c_int,
                cone_size_max as c_int,
                use_zeros as c_int,
                use_dcs as c_int,
            )
        });
    }
    /// Performs technology-independent rewriting of the AIG.
    ///
    /// # Arguments
    ///
    /// * `use_zeros`: Use zero-cost replacements (default: `false`).
    /// * `precompute`: Precompute subgraphs (default: `false`).
    pub fn rewrite(&mut self, use_zeros: bool, precompute: bool) {
        self.change_network(|ntk| unsafe {
            Abc_NtkRewrite(ntk, use_zeros as c_int, precompute as c_int)
        });
    }
    /// Performs combinational AIG rewriting.
    ///
    /// # Arguments
    ///
    /// * `cuts_max`: The maximum number of cuts at a node (default: `8`).
    /// * `subgraphs`: The maximum number of subgraphs tried (default: `5`).
    /// * `use_zeros`: Use zero-cost replacements (default: `false`).
    /// * `recycle`: Use cut recycling (default: `true`).
    pub fn drewrite(&mut self, cuts_max: usize, subgraphs: usize, use_zeros: bool, recycle: bool) {
        let lib = unsafe { Abc_FrameReadDarLib(self.frame) };
        let params = Dar_RwrPar_t {
            nCutsMax: cuts_max as c_int,
            nSubgMax: subgraphs as c_int,
            fUseZeros: use_zeros as c_int,
            fRecycle: recycle as c_int,
        };
        let params_ptr = &params as *const _ as *mut _;
        self.change_network_with(|ntk| unsafe { Abc_NtkDRewrite(lib, ntk, params_ptr) });
    }
    /// Performs combinational AIG refactoring.
    ///
    /// # Arguments
    ///
    /// * `mffc_min`: The minimum MFFC size to attempt refactoring (default: `2`).
    /// * `leaf_max`: The maximum number of cuts leaves (default: `12`).
    /// * `cuts_max`: The maximum number of cuts to try at a node (default: `5`).
    /// * `extend`: Extend the cut below MFCC (default: `false`).
    /// * `use_zeros`: Use zero-cost replacements (default: `false`).
    pub fn drefactor(
        &mut self,
        mffc_min: usize,
        leaf_max: usize,
        cuts_max: usize,
        extend: bool,
        use_zeros: bool,
    ) {
        let params = Dar_RefPar_t {
            nMffcMin: mffc_min as c_int,
            nLeafMax: leaf_max as c_int,
            nCutsMax: cuts_max as c_int,
            fExtend: extend as c_int,
            fUseZeros: use_zeros as c_int,
        };
        let params_ptr = &params as *const _ as *mut _;
        self.change_network_with(|ntk| unsafe { Abc_NtkDRefactor(ntk, params_ptr) });
    }
}

/// Tests for the ABC framework.
#[cfg(test)]
mod tests {
    use ::aiger::{AigerConstructor, Literal};

    use super::*;

    /// Helper function to construct a small aiger circuit with inputs, latches and outputs
    fn simple_aig() -> Aiger {
        let mut aig = AigerConstructor::new(2, 1).unwrap();

        let x = aig.add_input("x");
        let y = aig.add_input("y");
        let xy = aig.add_and(x, y);
        let l = aig.add_latch("l");

        aig.set_latch_next(l, xy);
        aig.set_latch_reset(l, Literal::TRUE);
        aig.add_output("out", l);

        aig.into_aiger()
    }

    /// Test setting and getting an aiger circuit in the ABC framework
    #[test]
    fn test_set_and_get() {
        let aig = simple_aig();
        let before = format!("{}", aig);

        let mut abc = Abc::new().unwrap();
        abc.set_aiger(&aig);
        let aig = abc.get_aiger();

        let after = format!("{}", aig);

        assert_eq!(before, after);
    }

    /// Test the operations in the ABC framework with different parameters.
    ///
    /// Only tests that no function panics, not that the output is correct.
    #[test]
    fn test_operations() {
        let aig = simple_aig();
        let mut abc = Abc::new().unwrap();
        abc.set_aiger(&aig);

        abc.zero();

        abc.balance(false, false);
        abc.balance(true, false);
        abc.balance(false, true);
        abc.balance(true, true);

        abc.rewrite(false, false);
        abc.rewrite(true, false);
        abc.rewrite(false, true);
        abc.rewrite(true, true);

        abc.resubstitute(8, 1);
        abc.resubstitute(8, 2);

        abc.refactor(10, 16, false, false);
        abc.refactor(10, 16, false, true);
        abc.refactor(10, 16, true, false);
        abc.refactor(10, 16, true, true);

        abc.drewrite(8, 5, false, false);
        abc.drewrite(8, 5, false, true);
        abc.drewrite(8, 5, true, false);
        abc.drewrite(8, 5, true, true);

        abc.drefactor(2, 12, 5, false, false);
        abc.drefactor(2, 12, 5, false, true);
        abc.drefactor(2, 12, 5, true, false);
        abc.drefactor(2, 12, 5, true, true);

        abc.get_aiger();
    }

    /// Test the balance operation in the ABC framework and that it actually balances a circuit.
    #[test]
    fn test_balance() {
        let mut aig = AigerConstructor::new(4, 0).unwrap();

        let x0 = aig.add_input("x0");
        let x1 = aig.add_input("x1");
        let x2 = aig.add_input("x2");
        let x3 = aig.add_input("x3");
        let x01 = aig.add_and(x0, x1);
        let x012 = aig.add_and(x01, x2);
        let x0123 = aig.add_and(x012, x3);
        aig.add_output("out", x0123);

        let aig = aig.into_aiger();
        let before = format!("{}", aig);
        assert_eq!(before, "aag 7 4 0 1 3\n2\n4\n6\n8\n14\n10 2 4\n12 6 10\n14 8 12\ni0 x0\ni1 x1\ni2 x2\ni3 x3\no0 out\n");

        let mut abc = Abc::new().unwrap();
        abc.set_aiger(&aig);
        abc.balance(false, false);
        let aig = abc.get_aiger();

        let after = format!("{}", aig);
        assert_eq!(after, "aag 7 4 0 1 3\n2\n4\n6\n8\n14\n10 2 4\n12 6 8\n14 10 12\ni0 x0\ni1 x1\ni2 x2\ni3 x3\no0 out\n");
    }
}
