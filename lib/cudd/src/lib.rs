//! Bindings to the CUDD library for binary decision diagrams.

#[doc(hidden)]
mod bindings;
mod cfile;

use std::borrow::Borrow;
use std::cmp::Ordering;
use std::convert::AsRef;
use std::error::Error;
use std::ffi::{CStr, CString};
use std::fmt;
use std::hash::Hash;
use std::ops::Index;
use std::os::raw::{c_char, c_int, c_uint, c_void};
use std::rc::Rc;

use bindings::*;

/// Internal wrapper for the CUDD manager. The manager
/// should only be accessed through an [`Rc`] pointer to
/// avoid dropping it while any BDDs created by it are still used.
#[derive(Debug)]
struct Manager {
    /// Raw pointer to the CUDD manager.
    manager: *mut DdManager,
    /// The error handler to call in case of errors.
    error_handler: fn(CuddError) -> (),
}

impl Drop for Manager {
    fn drop(&mut self) {
        unsafe { Cudd_Quit(self.manager) }
    }
}

/// A manager for BDDs using the CUDD framework.
#[derive(Debug)]
pub struct Cudd {
    /// Internal manager.
    manager: Rc<Manager>,
}

/// An error produced by the CUDD framework.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum CuddError {
    /// The framework is out of memory.
    MemoryOut,
    /// There are too many live nodes.
    TooManyNodes,
    /// The maximum memory limits have been exceeded.
    MaxMemExceeded,
    /// The framework has been terminated.
    Termination,
    /// There was an invalid argument.
    InvalidArg,
    /// An internal error has occurred.
    InternalError,
    /// An unexpected error has occurred.
    UnexpectedError,
    /// An operation on two BDDs from different managers has been attempted.
    DifferentManager,
}

impl fmt::Display for CuddError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "CUDD error: {}",
            match self {
                Self::MemoryOut => "Out of memory",
                Self::TooManyNodes => "Too many nodes",
                Self::MaxMemExceeded => "Maximum memory exceeded",
                Self::Termination => "Termination",
                Self::InvalidArg => "Invalid argument",
                Self::InternalError => "Internal error",
                Self::UnexpectedError => "Unexpected error",
                Self::DifferentManager => "Operands come from different manager",
            }
        )
    }
}

impl Error for CuddError {}

impl Manager {
    /// Checks the return value of a CUDD operation, and calls the error handler
    /// if an error has occurred.
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

    /// Checks if a BDD is from this manager.
    /// If this is the case, the manager pointer is returned,
    /// and otherwise the error handler is called.
    fn check_same_manager(&self, other: &Bdd) -> *mut DdManager {
        if self.manager != other.cudd.manager {
            (self.error_handler)(CuddError::DifferentManager);
        }
        self.manager
    }
}

impl Cudd {
    /// Creates a new CUDD manager.
    ///
    /// # Arguments
    ///
    /// * `num_vars`: The initial number of BDD variables.
    /// * `num_vars_z`: The initial number of ZDD variables.
    /// * `num_slots`: Initial size of the unique tables.
    /// * `cache_size`: Initial size of the cache.
    /// * `max_memory`: Target maximum memory occupation.
    /// * `error_handler`: A function which is called if an error occurs.
    ///
    /// If `max_memory` is 0, the function decides suitable values for
    /// the maximum size of the cache and for the limit for fast
    /// unique table growth based on the available memory.
    ///
    /// # Errors
    ///
    /// Returns an error if the CUDD framework could not be instantiated.
    pub fn new(
        num_vars: usize,
        num_vars_z: usize,
        num_slots: usize,
        cache_size: usize,
        max_memory: usize,
        error_handler: fn(CuddError) -> (),
    ) -> Result<Self, CuddError> {
        /*

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
            Ok(Self {
                manager: Rc::new(Manager {
                    manager,
                    error_handler,
                }),
            })
        }
    }

    /// The default error handler, which panics with the given error message.
    pub fn default_handler(error: CuddError) {
        panic!("{}", error)
    }

    /// Create a CUDD manager with default values.
    ///
    /// # Errors
    ///
    /// Returns an error if the CUDD framework could not be instantiated.
    pub fn default() -> Result<Self, CuddError> {
        Self::with_vars(0)
    }

    /// Create a CUDD manager with the given number of initial BDD variables
    /// and default values otherwise.
    ///
    /// # Errors
    ///
    /// Returns an error if the CUDD framework could not be instantiated.
    pub fn with_vars(num_vars: usize) -> Result<Self, CuddError> {
        Self::new(
            num_vars,
            0,
            CUDD_UNIQUE_SLOTS as usize,
            CUDD_CACHE_SLOTS as usize,
            0,
            Self::default_handler,
        )
    }

    /// Returns a new BDD variable.
    ///
    /// The new variable has an index equal to the largest previous index plus 1.
    ///
    /// Calls the set error handler if an error occurs.
    pub fn bdd_new_var(&self) -> Bdd {
        let node = unsafe { Cudd_bddNewVar(self.manager.manager) };
        self.manager.check_return_value(node as *const c_void);
        Bdd::new(&self.manager, node)
    }

    /// Returns the BDD variable with the given index.
    ///
    /// Retrieves the BDD variable if the given index if it already exists,
    /// or creates a new BDD variable.
    ///
    /// Calls the set error handler if an error occurs.
    pub fn bdd_var(&self, index: usize) -> Bdd {
        let node = unsafe { Cudd_bddIthVar(self.manager.manager, index as c_int) };
        self.manager.check_return_value(node as *const c_void);
        Bdd::new(&self.manager, node)
    }

    /// Returns the one constant of the manager.
    ///
    /// The one constant is common to ADDs and BDDs.
    ///
    /// Calls the set error handler if an error occurs.
    pub fn bdd_one(&self) -> Bdd {
        let node = unsafe { Cudd_ReadOne(self.manager.manager) };
        self.manager.check_return_value(node as *const c_void);
        Bdd::new(&self.manager, node)
    }

    /// Returns the logic zero constant of the manager.
    ///
    /// The logic zero constant is the complement of the one
    /// constant, and is distinct from the arithmetic zero.
    ///
    /// Calls the set error handler if an error occurs.
    pub fn bdd_zero(&self) -> Bdd {
        let node = unsafe { Cudd_ReadLogicZero(self.manager.manager) };
        self.manager.check_return_value(node as *const c_void);
        Bdd::new(&self.manager, node)
    }

    /// Returns a string with a Graphviz/DOT representation of the argument BDDs.
    ///
    /// The argument `in_names` is used for the names of the variables
    /// and the argument `out_names` for the names of the BDDs.
    ///
    /// Calls the set error handler if an error occurs.
    pub fn dump_dot<S: AsRef<str>>(&self, bdds: &[Bdd], in_names: &[S], out_names: &[S]) -> String {
        use std::io::{Read, Seek, SeekFrom, Write};

        for bdd in bdds {
            self.manager.check_same_manager(bdd);
        }

        let in_names_cstring: Vec<_> = in_names
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();
        let in_names_ptr: Vec<_> = in_names_cstring
            .iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();
        let out_names_cstring: Vec<_> = out_names
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();
        let out_names_ptr: Vec<_> = out_names_cstring
            .iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();
        let nodes: Vec<_> = bdds.iter().map(|b| b.node).collect();
        let n = bdds.len();

        // open a tempfile
        let mut f = cfile::tmpfile().unwrap();

        let retval = unsafe {
            Cudd_DumpDot(
                self.manager.manager,
                n as c_int,
                nodes.as_ptr() as *mut _,
                in_names_ptr.as_ptr() as *mut _,
                out_names_ptr.as_ptr() as *mut _,
                f.as_ptr(),
            )
        };
        self.manager.check_return_value(retval as *const c_void);

        // force to flush the stream
        f.flush().unwrap();
        let pos = f.position().unwrap();

        // seek to the beginning of stream
        assert_eq!(f.seek(SeekFrom::Start(0)).unwrap(), 0);

        let mut buffer = vec![0; pos as usize];
        f.read_exact(&mut buffer).unwrap();
        String::from_utf8(buffer).unwrap()
    }

    /// Calls the given method for dynamic reordering.
    ///
    /// The argument `minsize`, if non-zero, can be used to give a bound below
    /// which no reordering occurs.
    ///
    /// Changes the the variable order for all diagrams and clears the cache as
    /// side effects.
    ///
    /// Calls the set error handler if an error occurs.
    pub fn reduce_heap(&mut self, method: ReorderingMethod, minsize: usize) {
        let result =
            unsafe { Cudd_ReduceHeap(self.manager.manager, method.to_cudd(), minsize as c_int) };
        self.manager.check_return_value(result as *const c_void);
    }

    /// Enables automatic dynamic reordering of BDDs and ADDs.
    ///
    /// Parameter `method` is used to determine the method used for
    /// reordering. If [`ReorderingMethod::Same`] is passed, the method is unchanged.
    pub fn autodyn_enable(&mut self, method: ReorderingMethod) {
        unsafe { Cudd_AutodynEnable(self.manager.manager, method.to_cudd()) };
    }

    /// Disables automatic dynamic reordering.
    pub fn autodyn_disable(&mut self) {
        unsafe { Cudd_AutodynDisable(self.manager.manager) };
    }
}

/// A method for variable reordering.
#[derive(Debug, Copy, Clone)]
pub enum ReorderingMethod {
    /// This method causes no reordering.
    None,
    /// If passed to [`Cudd::autodyn_enable`], this method leaves the current method
    /// for automatic reordering unchanged. If passed to [`Cudd::reduce_heap`], this
    /// method causes the current method for automatic reordering to be used.
    Same,
    /// This method is an implementation of Rudell's sifting algorithm.
    ///
    /// A simplified version of sifting is as follows: Each variable is considered in
    /// turn. A variable is moved up and down in the order so that it takes all possible
    /// positions. The best position is identified and the variable is returned to that position.
    Sift,
    /// This is the converging variant of [`ReorderingMethod::Sift`].
    SiftConverge,
    /// This method implements a dynamic programming approach to exact reordering. It only
    /// stores one BDD at a time. Therefore, it is relatively efficient in terms of memory.
    /// Compared to other strategies, it is very slow, and is not recommended for more than
    /// 16 variables.
    Exact,
}

impl ReorderingMethod {
    /// Converts the reordering method to the CUDD enum variant.
    const fn to_cudd(self) -> Cudd_ReorderingType {
        match self {
            Self::Same => Cudd_ReorderingType_CUDD_REORDER_SAME,
            Self::None => Cudd_ReorderingType_CUDD_REORDER_NONE,
            Self::Sift => Cudd_ReorderingType_CUDD_REORDER_SIFT,
            Self::SiftConverge => Cudd_ReorderingType_CUDD_REORDER_SIFT_CONVERGE,
            Self::Exact => Cudd_ReorderingType_CUDD_REORDER_EXACT,
        }
    }
}

/// A binary decision diagram (BDD).
///
/// As BDDs implement the correspond and, or and not operations,
/// conjunction, disjunction and negation of BDDs can be performed
/// using the operators `&`, `|` and `!`.
/// Both the left-hand-side and right-hand-side of the operators can be references.
/// BDDs are not copy types, but cloning a BDD is cheap as only a reference count is increased.
///
/// # Examples
///
/// The following example uses BDDs to show that the equivalence
/// `!(x & y) == !x | !y` holds.
///
/// ```
/// # use cudd::Cudd;
/// let manager = Cudd::default().unwrap();
/// let x = &manager.bdd_new_var();
/// let y = &manager.bdd_new_var();
/// let lhs = !(x & y);
/// let rhs = !x | !y;
/// assert_eq!(lhs, rhs);
/// ```
#[derive(Debug)]
pub struct Bdd {
    /// Pointer to the manager for this BDD.
    cudd: Rc<Manager>,
    /// Raw pointer to the BDD node.
    node: *mut DdNode,
}

impl Drop for Bdd {
    fn drop(&mut self) {
        if !self.node.is_null() {
            unsafe { Cudd_RecursiveDeref(self.cudd.manager, self.node) };
        }
    }
}

impl Bdd {
    /// Returns the manager which was used to create this BDD.
    pub fn manager(&self) -> Cudd {
        Cudd {
            manager: Rc::clone(&self.cudd),
        }
    }

    /// Returns the unique node id for this BDD as an integer.
    pub fn node_id(&self) -> usize {
        self.node as usize
    }

    /// Creates a new wrapped BDD for the raw pointer node.
    ///
    /// Increments the reference count for the node by one.
    fn new(cudd: &Rc<Manager>, node: *mut DdNode) -> Self {
        if !node.is_null() {
            unsafe { Cudd_Ref(node) };
        }
        Self {
            cudd: Rc::clone(cudd),
            node,
        }
    }

    /// Returns the regular version of this BDD.
    pub fn regular(&self) -> Self {
        Self::new(&self.cudd, Cudd_Regular(self.node))
    }

    /// Returns whether this BDD is a constant, i.e. zero or one.
    pub fn is_constant(&self) -> bool {
        unsafe { Cudd_IsConstant(self.node) != 0 }
    }

    /// Returns whether this BDD is complemented.
    pub fn is_complement(&self) -> bool {
        Cudd_IsComplement(self.node)
    }

    /// Returns whether this BDD is constant zero.
    pub fn is_zero(&self) -> bool {
        self.is_constant() && self.is_complement()
    }

    /// Returns whether this BDD is constant one.
    pub fn is_one(&self) -> bool {
        self.is_constant() && !self.is_complement()
    }

    /// Performs an if-then-else operation with this BDD and the given operands,
    /// and returns the resulting BDD.
    ///
    /// Calls the set error handler if an error occurs or the BDDs come from different managers.
    pub fn ite(&self, g: &Self, h: &Self) -> Self {
        let mgr = self.cudd.check_same_manager(g);
        self.cudd.check_same_manager(h);
        let node = unsafe { Cudd_bddIte(mgr, self.node, g.node, h.node) };
        self.cudd.check_return_value(node as *const c_void);
        Self::new(&self.cudd, node)
    }

    /// Performs an if-then-else operation with this BDD and the given operands,
    /// and assigns the result to itself.
    ///
    /// Calls the set error handler if an error occurs or the BDDs come from different managers.
    pub fn ite_assign(&mut self, g: &Self, h: &Self) {
        let mgr = self.cudd.check_same_manager(g);
        self.cudd.check_same_manager(h);
        let node = unsafe { Cudd_bddIte(mgr, self.node, g.node, h.node) };
        self.cudd.check_return_value(node as *const c_void);
        unsafe { Cudd_Ref(node) };
        unsafe { Cudd_RecursiveDeref(mgr, self.node) };
        self.node = node;
    }

    /// Returns a factored form representation of this BDD with the given names.
    ///
    /// The factored form uses `&` for conjunction, `|` for disjunction
    /// and `!` for negation.  Caution must be exercised because the factored
    /// form may be exponentially larger than this BDD.
    pub fn factored_form_string<S: AsRef<str>>(&self, names: &[S]) -> String {
        let p_cstring: Vec<_> = names
            .iter()
            .map(|p| CString::new(p.as_ref()).unwrap())
            .collect();

        let p_ptr: Vec<_> = p_cstring
            .iter() // do NOT into_iter()
            .map(|arg| arg.as_ptr() as *mut c_char)
            .collect();

        let ff_cstring = unsafe {
            Cudd_FactoredFormString(
                self.cudd.manager,
                self.node,
                p_ptr.as_ptr() as *const *const _,
            )
        };
        let string = unsafe { CStr::from_ptr(ff_cstring).to_str().unwrap().to_string() };
        unsafe { Cudd_Free(ff_cstring as *mut c_void) };

        string
    }

    /// Returns an iterator that iterates over the paths of this BDD
    /// and returns the cube for each path.
    #[must_use]
    pub fn cube_iter(&self, num_vars: usize) -> CubeIter<'_> {
        CubeIter::new(self, num_vars)
    }

    /// Returns an iterator that iterates over the paths of this BDD
    /// and returns the BDD for each path.
    #[must_use]
    pub fn bdd_cube_iter(&self, num_vars: usize) -> BddCubeIter<'_> {
        BddCubeIter::new(self, num_vars)
    }

    /// Converts this BDD to another manager and returns the copy in the destination manager.
    ///
    /// The order of the variables in the two managers my be different.
    ///
    /// Calls the set error handler if an error occurs.
    pub fn transfer(&self, destination: &Cudd) -> Self {
        let node =
            unsafe { Cudd_bddTransfer(self.cudd.manager, destination.manager.manager, self.node) };
        self.cudd.check_return_value(node as *const c_void);
        Self::new(&destination.manager, node)
    }

    /// Returns a view into the node for this BDD.
    #[must_use]
    pub fn view(&self) -> BddView {
        if self.is_constant() {
            BddView::Constant
        } else {
            let var = unsafe { Cudd_NodeReadIndex(self.node) } as usize;
            let bdd_then = Self::new(&self.cudd, unsafe { Cudd_T(self.node) });
            let bdd_else = Self::new(&self.cudd, unsafe { Cudd_E(self.node) });
            BddView::InnerNode {
                var,
                bdd_then,
                bdd_else,
            }
        }
    }
}

/// A view into a node of a BDD.
#[derive(Debug)]
pub enum BddView {
    /// A constant node.
    Constant,
    /// An inner node.
    InnerNode {
        /// The variable in the node.
        var: usize,
        /// Then child of the node.
        bdd_then: Bdd,
        /// Else child of the node.
        bdd_else: Bdd,
    },
}

/// A value for a variable in a cube.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum CubeValue {
    /// The variable is not set, i.e. equal to 0.
    Unset = 0,
    /// The variable is set, i.e. equal to 1.
    Set = 1,
    /// The variable is unspecified, i.e. may be 0 or 1.
    Unspecified = 2,
}

impl CubeValue {
    /// Creates a cube value from a CUDD cube value.
    fn from_cudd(val: c_int) -> Self {
        match val {
            0 => Self::Unset,
            1 => Self::Set,
            2 => Self::Unspecified,
            _ => panic!("invalid cube value"),
        }
    }

    /// Transform this cube value into a CUDD cube value.
    const fn to_cudd(self) -> c_int {
        match self {
            Self::Unset => 0,
            Self::Set => 1,
            Self::Unspecified => 2,
        }
    }
}

impl fmt::Display for CubeValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::Unset => '0',
                Self::Set => '1',
                Self::Unspecified => '-',
            }
        )
    }
}

/// A cube that stores variable values in a path through a BDD.
#[derive(Debug)]
pub struct Cube {
    /// The values of the variables in this cube.
    cube: Vec<CubeValue>,
}

impl Cube {
    /// Create a new cube from the raw cube pointer with the given number of variables.
    ///
    /// The pointer must point to a valid memory region of the given size with valid cube values.
    fn make(cube_ptr: *mut c_int, num_vars: usize) -> Self {
        let slice = unsafe { std::slice::from_raw_parts(cube_ptr, num_vars) };
        let cube = Self {
            cube: slice.iter().cloned().map(CubeValue::from_cudd).collect(),
        };
        cube
    }

    /// Returns a rperesentation of this cube as a vector of CUDD cube values.
    fn to_array(&self) -> Vec<c_int> {
        self.cube.iter().map(|v| v.to_cudd()).collect()
    }

    /// Returns an iterator over the values of this cube.
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

/// An iterator over cubes corresponding to the paths in a source BDD.
pub struct CubeIter<'a> {
    /// The source BDD.
    bdd: &'a Bdd,
    /// The number of variables along each path.
    num_vars: usize,
    /// Raw pointer to the CUDD generator the next cube.
    gen: *mut DdGen,
    /// The next cube to return, or `None` if all cubes have been returned.
    next_cube: Option<Cube>,
}

impl<'a> CubeIter<'a> {
    /// Initialize the iterator with the first cube.
    fn init(&mut self) {
        let mut cube_ptr = std::ptr::null_mut();
        // we don't need the value, but have to provide a memory location where it is stored
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

    /// Creates a new iterator with the given source BDD and number of variables.
    fn new(bdd: &'a Bdd, num_vars: usize) -> Self {
        let mut iter = Self {
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

/// An iterator over BDDs corresponding to the paths in a source BDD.
pub struct BddCubeIter<'a> {
    /// The cube iterator that is used to iterate over paths.
    cube_iter: CubeIter<'a>,
}

impl<'a> BddCubeIter<'a> {
    /// Creates a new iterator with the given source BDD and number of variables.
    fn new(bdd: &'a Bdd, num_vars: usize) -> Self {
        BddCubeIter {
            cube_iter: CubeIter::new(bdd, num_vars),
        }
    }
}

impl<'a> Iterator for BddCubeIter<'a> {
    type Item = Bdd;

    fn next(&mut self) -> Option<Self::Item> {
        self.cube_iter.next().map(|cube| {
            let bdd = self.cube_iter.bdd;
            let mgr = bdd.cudd.manager;
            let array = cube.to_array();
            let node = unsafe { Cudd_CubeArrayToBdd(mgr, array.as_ptr() as *mut _) };
            bdd.cudd.check_return_value(node as *const c_void);
            Bdd::new(&bdd.cudd, node)
        })
    }
}

impl fmt::Display for Bdd {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let ff_cstring =
            unsafe { Cudd_FactoredFormString(self.cudd.manager, self.node, std::ptr::null()) };
        let string = unsafe { CStr::from_ptr(ff_cstring).to_str().unwrap().to_string() };
        unsafe { Cudd_Free(ff_cstring as *mut c_void) };
        write!(f, "{}", string)
    }
}

impl Hash for Bdd {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.node.hash(state);
    }
}

impl PartialEq for Bdd {
    fn eq(&self, other: &Self) -> bool {
        self.cudd.check_same_manager(other);
        self.node == other.node
    }
}
impl Eq for Bdd {}

impl PartialOrd for Bdd {
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

impl Clone for Bdd {
    fn clone(&self) -> Self {
        Self::new(&self.cudd, self.node)
    }
}

macro_rules! not_impl {
    ($t:ty) => {
        impl std::ops::Not for $t {
            type Output = Bdd;

            fn not(self) -> Self::Output {
                let node = Cudd_Not(self.node);
                Bdd::new(&self.cudd, node)
            }
        }
    };
}

not_impl!(Bdd);
not_impl!(&Bdd);

macro_rules! and_impl {
    ($t:ty) => {
        impl<R: Borrow<Bdd>> std::ops::BitAnd<R> for $t {
            type Output = Bdd;

            fn bitand(self, rhs: R) -> Self::Output {
                let rhs = rhs.borrow();
                let mgr = self.cudd.check_same_manager(rhs);
                let node = unsafe { Cudd_bddAnd(mgr, self.node, rhs.node) };
                self.cudd.check_return_value(node as *const c_void);
                Bdd::new(&self.cudd, node)
            }
        }
    };
}

and_impl!(Bdd);
and_impl!(&Bdd);

impl<R: Borrow<Bdd>> std::ops::BitAndAssign<R> for Bdd {
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
        impl<R: Borrow<Bdd>> std::ops::BitOr<R> for $t {
            type Output = Bdd;

            fn bitor(self, rhs: R) -> Self::Output {
                let rhs = rhs.borrow();
                let mgr = self.cudd.check_same_manager(rhs);
                let node = unsafe { Cudd_bddOr(mgr, self.node, rhs.node) };
                self.cudd.check_return_value(node as *const c_void);
                Bdd::new(&self.cudd, node)
            }
        }
    };
}

or_impl!(Bdd);
or_impl!(&Bdd);

impl<R: Borrow<Bdd>> std::ops::BitOrAssign<R> for Bdd {
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

/// Tests for the CUDD framework.
#[cfg(test)]
mod tests {
    use super::*;

    /// Test that one and zero are distinct and complementary BDDs.
    #[test]
    fn test_one_zero() {
        let cudd = Cudd::default().unwrap();
        let one = cudd.bdd_one();
        let zero = cudd.bdd_zero();
        assert_ne!(one, zero);
        assert_eq!(!(&one), zero);
        assert_eq!(one, !(&zero));
    }

    /// Test De Morgan's law using BDDs.
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
