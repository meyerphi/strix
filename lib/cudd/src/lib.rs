mod bindings;
mod cfile;

use std::borrow::Borrow;
use std::cmp::Ordering;
use std::convert::AsRef;
use std::ffi::{CStr, CString};
use std::fmt;
use std::hash::Hash;
use std::ops::Index;
use std::os::raw::{c_char, c_int, c_uint, c_void};
use std::rc::Rc;

use bindings::*;

#[derive(Debug)]
struct Manager {
    manager: *mut DdManager,
    error_handler: fn(CuddError) -> (),
}

impl Drop for Manager {
    fn drop(&mut self) {
        unsafe { Cudd_Quit(self.manager) }
    }
}
#[derive(Debug)]
pub struct Cudd {
    manager: Rc<Manager>,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum CuddError {
    MemoryOut,
    TooManyNodes,
    MaxMemExceeded,
    Termination,
    InvalidArg,
    InternalError,
    UnexpectedError,
    DifferentManager,
}

impl fmt::Display for CuddError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "CUDD error: {}",
            match self {
                CuddError::MemoryOut => "Out of memory",
                CuddError::TooManyNodes => "Too many nodes",
                CuddError::MaxMemExceeded => "Maximum memory exceeded",
                CuddError::Termination => "Termination",
                CuddError::InvalidArg => "Invalid argument",
                CuddError::InternalError => "Internal error",
                CuddError::UnexpectedError => "Unexpected error",
                CuddError::DifferentManager => "Operands come from different manager",
            }
        )
    }
}

impl Manager {
    #[allow(non_snake_case)]
    #[allow(non_upper_case_globals)]
    fn check_return_value(&self, result: *const c_void) {
        if result.is_null() {
            let error_code = unsafe { Cudd_ReadErrorCode(self.manager) };
            let error = match error_code {
                Cudd_ErrorType_CUDD_MEMORY_OUT => CuddError::MemoryOut,
                Cudd_ErrorType_CUDD_TOO_MANY_NODES => CuddError::TooManyNodes,
                Cudd_ErrorType_CUDD_MAX_MEM_EXCEEDED => CuddError::MaxMemExceeded,
                Cudd_ErrorType_CUDD_TERMINATION => CuddError::Termination,
                Cudd_ErrorType_CUDD_INVALID_ARG => CuddError::InvalidArg,
                Cudd_ErrorType_CUDD_INTERNAL_ERROR => CuddError::InternalError,
                _ => CuddError::UnexpectedError,
            };
            (self.error_handler)(error);
        }
    }

    fn check_same_manager(&self, bdd: &BDD) -> *mut DdManager {
        if self.manager != bdd.cudd.manager {
            (self.error_handler)(CuddError::DifferentManager);
        }
        self.manager
    }
}

impl Cudd {
    pub fn new(
        num_vars: usize,
        num_vars_z: usize,
        num_slots: usize,
        cache_size: usize,
        max_memory: usize,
        error_handler: fn(CuddError) -> (),
    ) -> Result<Cudd, CuddError> {
        /*
        If maxMemory is 0, Cudd_Init decides suitable values for
        the maximum size of the cache and for the limit for fast
        unique table growth based on the available memory.
        */
        let manager = unsafe {
            Cudd_Init(
                num_vars as c_uint,
                num_vars_z as c_uint,
                num_slots as c_uint,
                cache_size as c_uint,
                max_memory as size_t,
            )
        };
        if manager.is_null() {
            Err(CuddError::MemoryOut)
        } else {
            Ok(Cudd {
                manager: Rc::new(Manager {
                    manager,
                    error_handler,
                }),
            })
        }
    }

    pub fn default_handler(error: CuddError) {
        panic!("{}", error)
    }

    pub fn default() -> Result<Cudd, CuddError> {
        Cudd::with_vars(0)
    }

    pub fn with_vars(num_vars: usize) -> Result<Cudd, CuddError> {
        Cudd::new(
            num_vars,
            0,
            CUDD_UNIQUE_SLOTS as usize,
            CUDD_CACHE_SLOTS as usize,
            0,
            Cudd::default_handler,
        )
    }

    pub fn info(&self) {
        let retval = unsafe { Cudd_PrintInfo(self.manager.manager, stdout) };
        self.manager.check_return_value(retval as *const c_void);
    }

    pub fn bdd_new_var(&self) -> BDD {
        let node = unsafe { Cudd_bddNewVar(self.manager.manager) };
        self.manager.check_return_value(node as *const c_void);
        BDD::new(&self.manager, node)
    }

    pub fn bdd_var(&self, index: usize) -> BDD {
        let node = unsafe { Cudd_bddIthVar(self.manager.manager, index as c_int) };
        self.manager.check_return_value(node as *const c_void);
        BDD::new(&self.manager, node)
    }

    pub fn bdd_one(&self) -> BDD {
        let node = unsafe { Cudd_ReadOne(self.manager.manager) };
        self.manager.check_return_value(node as *const c_void);
        BDD::new(&self.manager, node)
    }

    pub fn bdd_zero(&self) -> BDD {
        let node = unsafe { Cudd_ReadLogicZero(self.manager.manager) };
        self.manager.check_return_value(node as *const c_void);
        BDD::new(&self.manager, node)
    }

    pub fn dump_dot<S: AsRef<str>>(&self, bdds: &[BDD], inames: &[S], onames: &[S]) -> String {
        use std::io::{Read, Seek, SeekFrom, Write};

        for bdd in bdds {
            self.manager.check_same_manager(bdd);
        }

        let inames_cstring: Vec<_> = inames
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();
        let inames_ptr: Vec<_> = inames_cstring
            .iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();
        let onames_cstring: Vec<_> = onames
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();
        let onames_ptr: Vec<_> = onames_cstring
            .iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();
        let nodes: Vec<_> = bdds.iter().map(|b| b.node).collect();
        let n = bdds.len();

        // open a tempfile
        let mut f = cfile::tmpfile().unwrap();

        unsafe {
            Cudd_DumpDot(
                self.manager.manager,
                n as c_int,
                nodes.as_ptr() as *mut _,
                inames_ptr.as_ptr() as *mut _,
                onames_ptr.as_ptr() as *mut _,
                f.as_ptr(),
            )
        };

        // force to flush the stream
        f.flush().unwrap();
        let pos = f.position().unwrap();

        // seek to the beginning of stream
        assert_eq!(f.seek(SeekFrom::Start(0)).unwrap(), 0);

        let mut buffer = vec![0; pos as usize];
        f.read_exact(&mut buffer).unwrap();
        String::from_utf8(buffer).unwrap()
    }

    pub fn reduce_heap(&mut self, heuristic: ReorderingType, minsize: usize) {
        let result =
            unsafe { Cudd_ReduceHeap(self.manager.manager, heuristic.to_cudd(), minsize as c_int) };
        self.manager.check_return_value(result as *const c_void);
    }

    pub fn autodyn_enable(&mut self, heuristic: ReorderingType) {
        unsafe { Cudd_AutodynEnable(self.manager.manager, heuristic.to_cudd()) };
    }

    pub fn autodyn_disable(&mut self) {
        unsafe { Cudd_AutodynDisable(self.manager.manager) };
    }
}

pub enum ReorderingType {
    Same,
    None,
    Sift,
    SiftConverge,
    Exact,
}

impl ReorderingType {
    fn to_cudd(&self) -> Cudd_ReorderingType {
        match self {
            ReorderingType::Same => Cudd_ReorderingType_CUDD_REORDER_SAME,
            ReorderingType::None => Cudd_ReorderingType_CUDD_REORDER_NONE,
            ReorderingType::Sift => Cudd_ReorderingType_CUDD_REORDER_SIFT,
            ReorderingType::SiftConverge => Cudd_ReorderingType_CUDD_REORDER_SIFT_CONVERGE,
            ReorderingType::Exact => Cudd_ReorderingType_CUDD_REORDER_EXACT,
        }
    }
}

#[derive(Debug)]
pub struct BDD {
    cudd: Rc<Manager>,
    node: *mut DdNode,
}

impl Drop for BDD {
    fn drop(&mut self) {
        if !self.node.is_null() {
            unsafe { Cudd_RecursiveDeref(self.cudd.manager, self.node) };
        }
    }
}

impl BDD {
    fn new(cudd: &Rc<Manager>, node: *mut DdNode) -> BDD {
        if !node.is_null() {
            unsafe { Cudd_Ref(node) };
        }
        BDD {
            cudd: Rc::clone(cudd),
            node,
        }
    }

    pub fn get_regular_node(&self) -> BDD {
        BDD::new(&self.cudd, Cudd_Regular(self.node))
    }

    pub fn is_constant(&self) -> bool {
        unsafe { Cudd_IsConstant(self.node) != 0 }
    }

    pub fn is_complement(&self) -> bool {
        Cudd_IsComplement(self.node)
    }

    pub fn is_zero(&self) -> bool {
        self.is_constant() && self.is_complement()
    }

    pub fn is_one(&self) -> bool {
        self.is_constant() && !self.is_complement()
    }

    pub fn ite(&self, g: &BDD, h: &BDD) -> BDD {
        let mgr = self.cudd.check_same_manager(g);
        self.cudd.check_same_manager(h);
        let node = unsafe { Cudd_bddIte(mgr, self.node, g.node, h.node) };
        self.cudd.check_return_value(node as *const c_void);
        BDD::new(&self.cudd, node)
    }

    pub fn ite_assign(&mut self, g: &BDD, h: &BDD) {
        let mgr = self.cudd.check_same_manager(g);
        self.cudd.check_same_manager(h);
        let node = unsafe { Cudd_bddIte(mgr, self.node, g.node, h.node) };
        self.cudd.check_return_value(node as *const c_void);
        unsafe { Cudd_Ref(node) };
        unsafe { Cudd_RecursiveDeref(mgr, self.node) };
        self.node = node;
    }

    pub fn factored_form_string<S: AsRef<str>>(&self, names: &[S]) -> String {
        let p_cstring: Vec<_> = names
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();

        let p_ptr: Vec<_> = p_cstring
            .iter() // do NOT into_iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();

        let cstring = unsafe {
            Cudd_FactoredFormString(
                self.cudd.manager,
                self.node,
                p_ptr.as_ptr() as *const *const _,
            )
        };
        let string = unsafe { CStr::from_ptr(cstring).to_str().unwrap().to_string() };
        unsafe { Cudd_Free(cstring as *mut c_void) };

        string
    }

    pub fn cube_iter(&self, num_vars: usize) -> CubeIter<'_> {
        CubeIter::new(&self, num_vars)
    }

    pub fn bdd_cube_iter(&self, num_vars: usize) -> BddCubeIter<'_> {
        BddCubeIter::new(&self, num_vars)
    }

    pub fn transfer(&self, destination: &Cudd) -> BDD {
        let node =
            unsafe { Cudd_bddTransfer(self.cudd.manager, destination.manager.manager, self.node) };
        self.cudd.check_return_value(node as *const c_void);
        BDD::new(&destination.manager, node)
    }

    pub fn view(&self) -> BddView {
        if self.is_constant() {
            BddView::Constant
        } else {
            let var = unsafe { Cudd_NodeReadIndex(self.node) } as usize;
            let bdd_then = BDD::new(&self.cudd, unsafe { Cudd_T(self.node) });
            let bdd_else = BDD::new(&self.cudd, unsafe { Cudd_E(self.node) });
            BddView::InnerNode {
                var,
                bdd_then,
                bdd_else,
            }
        }
    }
}

#[derive(Debug)]
pub enum BddView {
    Constant,
    InnerNode {
        var: usize,
        bdd_then: BDD,
        bdd_else: BDD,
    },
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum CubeValue {
    Unset = 0,
    Set = 1,
    Unspecified = 2,
}

impl CubeValue {
    fn from_cudd(val: c_int) -> Self {
        match val {
            0 => CubeValue::Unset,
            1 => CubeValue::Set,
            2 => CubeValue::Unspecified,
            _ => panic!("invalid cube value"),
        }
    }

    fn to_cudd(self) -> c_int {
        match self {
            CubeValue::Unset => 0,
            CubeValue::Set => 1,
            CubeValue::Unspecified => 2,
        }
    }
}

impl fmt::Display for CubeValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                CubeValue::Unset => '0',
                CubeValue::Set => '1',
                CubeValue::Unspecified => '-',
            }
        )
    }
}

#[derive(Debug)]
pub struct Cube {
    cube: Vec<CubeValue>,
}

impl Cube {
    fn make(cube_ptr: *mut c_int, num_vars: usize) -> Cube {
        let slice = unsafe { std::slice::from_raw_parts(cube_ptr, num_vars) };
        let cube = Cube {
            cube: slice.iter().cloned().map(CubeValue::from_cudd).collect(),
        };
        cube
    }

    fn to_array(&self) -> Vec<c_int> {
        self.cube.iter().map(|v| v.to_cudd()).collect()
    }

    pub fn iter(&self) -> impl Iterator<Item = &'_ CubeValue> {
        self.cube.iter()
    }
}

impl Index<usize> for Cube {
    type Output = CubeValue;

    fn index(&self, index: usize) -> &Self::Output {
        &self.cube[index]
    }
}

impl fmt::Display for Cube {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[")?;
        for val in &self.cube {
            write!(f, "{}", val)?;
        }
        write!(f, "]")?;
        Ok(())
    }
}

pub struct CubeIter<'a> {
    bdd: &'a BDD,
    num_vars: usize,
    gen: *mut DdGen,
    next_cube: Option<Cube>,
}

impl<'a> CubeIter<'a> {
    fn init(&mut self) {
        let mut cube_ptr = std::ptr::null_mut();
        let mut value = std::mem::MaybeUninit::<CUDD_VALUE_TYPE>::uninit();
        self.gen = unsafe {
            Cudd_FirstCube(
                self.bdd.cudd.manager,
                self.bdd.node,
                &mut cube_ptr,
                value.as_mut_ptr(),
            )
        };
        self.next_cube = Some(Cube::make(cube_ptr, self.num_vars));
    }

    fn new(bdd: &'a BDD, num_vars: usize) -> Self {
        let mut iter = CubeIter {
            bdd,
            num_vars,
            gen: std::ptr::null_mut(),
            next_cube: None,
        };
        iter.init();
        iter
    }
}

impl<'a> Drop for CubeIter<'a> {
    fn drop(&mut self) {
        unsafe { Cudd_GenFree(self.gen) };
    }
}

impl<'a> Iterator for CubeIter<'a> {
    type Item = Cube;

    fn next(&mut self) -> Option<Self::Item> {
        match self.next_cube.take() {
            None => None,
            Some(cube) => {
                let mut cube_ptr = std::ptr::null_mut();
                let mut value = std::mem::MaybeUninit::<CUDD_VALUE_TYPE>::uninit();
                unsafe { Cudd_NextCube(self.gen, &mut cube_ptr, value.as_mut_ptr()) };
                if unsafe { Cudd_IsGenEmpty(self.gen) } == 1 {
                    self.next_cube = None;
                } else {
                    self.next_cube = Some(Cube::make(cube_ptr, self.num_vars));
                }
                Some(cube)
            }
        }
    }
}

pub struct BddCubeIter<'a> {
    cube_iter: CubeIter<'a>,
}

impl<'a> BddCubeIter<'a> {
    fn new(bdd: &'a BDD, num_vars: usize) -> Self {
        BddCubeIter {
            cube_iter: CubeIter::new(bdd, num_vars),
        }
    }
}

impl<'a> Iterator for BddCubeIter<'a> {
    type Item = BDD;

    fn next(&mut self) -> Option<Self::Item> {
        self.cube_iter.next().map(|cube| {
            let bdd = self.cube_iter.bdd;
            let mgr = bdd.cudd.manager;
            let array = cube.to_array();
            let node = unsafe { Cudd_CubeArrayToBdd(mgr, array.as_ptr() as *mut _) };
            bdd.cudd.check_return_value(node as *const c_void);
            BDD::new(&bdd.cudd, node)
        })
    }
}

impl fmt::Display for BDD {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let cstring =
            unsafe { Cudd_FactoredFormString(self.cudd.manager, self.node, std::ptr::null()) };
        let string = unsafe { CStr::from_ptr(cstring).to_str().unwrap().to_string() };
        unsafe { Cudd_Free(cstring as *mut c_void) };
        write!(f, "{}", string)
    }
}

impl Hash for BDD {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.node.hash(state);
    }
}

impl PartialEq for BDD {
    fn eq(&self, other: &Self) -> bool {
        self.cudd.check_same_manager(other);
        self.node == other.node
    }
}
impl Eq for BDD {}

impl PartialOrd for BDD {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        let mgr = self.cudd.check_same_manager(other);
        if self.node == other.node {
            Some(Ordering::Equal)
        } else if unsafe { Cudd_bddLeq(mgr, self.node, other.node) } == 1 {
            Some(Ordering::Less)
        } else if unsafe { Cudd_bddLeq(mgr, other.node, self.node) } == 1 {
            Some(Ordering::Greater)
        } else {
            None
        }
    }
}

impl Clone for BDD {
    fn clone(&self) -> Self {
        BDD::new(&self.cudd, self.node)
    }
}

macro_rules! not_impl {
    ($t:ty) => {
        impl std::ops::Not for $t {
            type Output = BDD;

            fn not(self) -> Self::Output {
                let node = Cudd_Not(self.node);
                BDD::new(&self.cudd, node)
            }
        }
    };
}

not_impl!(BDD);
not_impl!(&BDD);

macro_rules! and_impl {
    ($t:ty) => {
        impl<R: Borrow<BDD>> std::ops::BitAnd<R> for $t {
            type Output = BDD;

            fn bitand(self, rhs: R) -> Self::Output {
                let rhs = rhs.borrow();
                let mgr = self.cudd.check_same_manager(rhs);
                let node = unsafe { Cudd_bddAnd(mgr, self.node, rhs.node) };
                self.cudd.check_return_value(node as *const c_void);
                BDD::new(&self.cudd, node)
            }
        }
    };
}

and_impl!(BDD);
and_impl!(&BDD);

impl<R: Borrow<BDD>> std::ops::BitAndAssign<R> for BDD {
    fn bitand_assign(&mut self, rhs: R) {
        let rhs = rhs.borrow();
        let mgr = self.cudd.check_same_manager(rhs);
        let node = unsafe { Cudd_bddAnd(mgr, self.node, rhs.node) };
        self.cudd.check_return_value(node as *const c_void);
        unsafe { Cudd_Ref(node) };
        unsafe { Cudd_RecursiveDeref(mgr, self.node) };
        self.node = node;
    }
}

macro_rules! or_impl {
    ($t:ty) => {
        impl<R: Borrow<BDD>> std::ops::BitOr<R> for $t {
            type Output = BDD;

            fn bitor(self, rhs: R) -> Self::Output {
                let rhs = rhs.borrow();
                let mgr = self.cudd.check_same_manager(rhs);
                let node = unsafe { Cudd_bddOr(mgr, self.node, rhs.node) };
                self.cudd.check_return_value(node as *const c_void);
                BDD::new(&self.cudd, node)
            }
        }
    };
}

or_impl!(BDD);
or_impl!(&BDD);

impl<R: Borrow<BDD>> std::ops::BitOrAssign<R> for BDD {
    fn bitor_assign(&mut self, rhs: R) {
        let rhs = rhs.borrow();
        let mgr = self.cudd.check_same_manager(rhs);
        let node = unsafe { Cudd_bddOr(mgr, self.node, rhs.node) };
        self.cudd.check_return_value(node as *const c_void);
        unsafe { Cudd_Ref(node) };
        unsafe { Cudd_RecursiveDeref(mgr, self.node) };
        self.node = node;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_one_zero() {
        let cudd = Cudd::default().unwrap();
        let one = cudd.bdd_one();
        let zero = cudd.bdd_zero();
        assert_ne!(one, zero);
        assert_eq!(!(&one), zero);
        assert_eq!(one, !(&zero));
    }

    #[test]
    fn test_de_morgan() {
        let cudd = Cudd::default().unwrap();
        let bdd1 = cudd.bdd_new_var();
        let bdd2 = cudd.bdd_new_var();
        let f1 = !((&bdd1) & (&bdd2));
        let f2 = (!bdd1) | (!bdd2);
        assert_eq!(f1, f2);
    }
}
