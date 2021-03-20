//! Bindings to the ABC library with a selective set of functions
//! for rewriting aiger circuits.

mod bindings;

use std::fmt;
use std::os::raw::c_int;

use ::aiger::Aiger;

use bindings::*;

#[derive(Debug)]
pub struct Abc {
    frame: *mut Abc_Frame_t,
}

impl Drop for Abc {
    fn drop(&mut self) {
        unsafe { Abc_Stop(self.frame) }
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum AbcError {
    MemoryOut,
}

impl fmt::Display for AbcError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "ABC error: {}",
            match self {
                AbcError::MemoryOut => "Out of memory",
            }
        )
    }
}

impl Abc {
    pub fn new() -> Result<Abc, AbcError> {
        let frame = unsafe { Abc_Start() };
        if frame.is_null() {
            Err(AbcError::MemoryOut)
        } else {
            Ok(Abc { frame })
        }
    }

    pub fn set_aiger(&mut self, aiger: &Aiger) {
        let aiger_ptr = unsafe { aiger.raw_ptr() } as *mut bindings::aiger;
        let ntk = unsafe { Io_LoadAiger(aiger_ptr, true as c_int) };
        unsafe { Abc_FrameReplaceNetwork(self.frame, ntk) };
    }
    pub fn get_aiger(&mut self) -> Aiger {
        let ntk = unsafe { Abc_FrameReadNtk(self.frame) };
        let aiger_ptr = unsafe { Io_StoreAiger(ntk, true as c_int) };
        unsafe { Aiger::from_raw(aiger_ptr as *mut ::aiger::AigerRaw) }
    }

    fn get_network(&self) -> *mut Abc_Ntk_t {
        unsafe { Abc_FrameReadNtk(self.frame) }
    }
    fn set_network(&mut self, ntk: *mut Abc_Ntk_t) {
        unsafe { Abc_FrameReplaceNetwork(self.frame, ntk) };
    }

    fn change_network<F>(&mut self, f: F)
    where
        F: FnOnce(*mut Abc_Ntk_t) -> c_int,
    {
        let ntk = self.get_network();
        let result = f(ntk);
        assert!(result != 0);
    }
    fn change_network_with<F>(&mut self, f: F)
    where
        F: FnOnce(*mut Abc_Ntk_t) -> *mut Abc_Ntk_t,
    {
        let ntk = self.get_network();
        let ntk_new = f(ntk);
        self.set_network(ntk_new);
    }

    pub fn network_size(&self) -> usize {
        let ntk = unsafe { Abc_FrameReadNtk(self.frame) };
        let nodes = unsafe { Abc_NtkNetworkSize(ntk) };
        nodes as usize
    }

    pub fn zero(&mut self) {
        self.change_network_with(|ntk| unsafe { Abc_NtkRestrashZero(ntk) });
    }
    pub fn balance(&mut self, duplicative: bool, selective: bool) {
        self.change_network_with(|ntk| unsafe {
            Abc_NtkBalance(ntk, duplicative as c_int, selective as c_int)
        });
    }
    pub fn resubstitute(&mut self, cuts_max: usize, nodes_max: usize) {
        assert!((4..=16).contains(&cuts_max));
        assert!((0..=3).contains(&nodes_max));
        self.change_network(|ntk| unsafe {
            Abc_NtkResubstitute(ntk, cuts_max as c_int, nodes_max as c_int)
        });
    }
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
    pub fn rewrite(&mut self, use_zeros: bool, precompute: bool) {
        self.change_network(|ntk| unsafe {
            Abc_NtkRewrite(ntk, use_zeros as c_int, precompute as c_int)
        });
    }
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
#[cfg(test)]
mod tests {
    use ::aiger::{AigerConstructor, Literal};

    use super::*;

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

    #[test]
    fn test_read_write() {
        let aig = simple_aig();
        let before = format!("{}", aig);

        let mut abc = Abc::new().unwrap();
        abc.set_aiger(&aig);
        let aig = abc.get_aiger();

        let after = format!("{}", aig);

        assert_eq!(before, after);
    }

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
