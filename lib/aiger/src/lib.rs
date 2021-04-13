//! Low-level bindings to the aiger library and a high-level aiger constructor.

#[doc(hidden)]
mod bindings;

use std::collections::hash_map::Entry;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::fmt;
use std::io::{self, Read, Write};
use std::os::raw::{c_char, c_int, c_uint, c_void};

use bindings::*;

/// The raw pointer type for an aiger.
pub type AigerRaw = aiger;

/// The false literal.
pub const AIGER_FALSE: c_uint = aiger_false;
/// The true literal.
pub const AIGER_TRUE: c_uint = aiger_true;

/// Returns the sign of a literal, i.e. whether it is complemented or not.
pub const fn aiger_sign(lit: c_uint) -> c_uint {
    lit & 1
}
/// Strips the sign of a literal, i.e. returns the uncomplemented version.
pub const fn aiger_strip(lit: c_uint) -> c_uint {
    lit & !1
}
/// Inverts a literal, i.e. toggles the complement state.
pub const fn aiger_not(lit: c_uint) -> c_uint {
    lit ^ 1
}
/// Returns the uncomplemented literal associated to a variable.
pub const fn aiger_var2lit(var: c_uint) -> c_uint {
    var << 1
}
/// Returns the variable associated to a literal.
pub const fn aiger_lit2var(lit: c_uint) -> c_uint {
    lit >> 1
}

/// An and-inverter graph (aiger) circuit.
#[derive(Debug)]
pub struct Aiger {
    /// The underlying raw pointer for the C interface.
    aiger: *mut AigerRaw,
}

impl Drop for Aiger {
    fn drop(&mut self) {
        unsafe { aiger_reset(self.aiger) };
    }
}

/// The mode for writing the aiger circuit.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum AigerMode {
    /// Option to write the circuit in ASCII format.
    Ascii,
    /// Option to write the circuit in compressed binary format.
    Binary,
}

impl AigerMode {
    /// Converts the aiger mode to the enum option in the C interface.
    fn as_aiger_mode(self) -> aiger_mode {
        match self {
            Self::Ascii => aiger_mode_aiger_ascii_mode,
            Self::Binary => aiger_mode_aiger_binary_mode,
        }
    }
}

impl Aiger {
    /// Returns a new aiger circuit.
    ///
    /// # Errors
    ///
    /// Returns an error if the aiger circuit could not be initialized.
    pub fn new() -> Result<Self, &'static str> {
        let aiger = unsafe { aiger_init() };
        if aiger.is_null() {
            Err("Failed to initialize aiger")
        } else {
            Ok(Self { aiger })
        }
    }

    /// Construct a wrapped aiger circuit from a raw pointer.
    ///
    /// # Safety
    ///
    /// The pointer `aiger` must point to a valid aiger struct.
    pub unsafe fn from_raw(aiger: *mut AigerRaw) -> Self {
        Self { aiger }
    }

    /// Extract the raw pointer in this wrapped aiger circuit.
    ///
    /// # Safety
    ///
    /// This struct must not be dropped while the pointer is used.
    pub unsafe fn raw_ptr(&self) -> *mut AigerRaw {
        self.aiger
    }

    /// The maximum variable index.
    /// The maximum literal value is then `2*maxvar+1`.
    pub fn maxvar(&self) -> c_uint {
        unsafe { (*self.aiger).maxvar }
    }

    /// The number of inputs.
    pub fn num_inputs(&self) -> c_uint {
        unsafe { (*self.aiger).num_inputs }
    }

    /// The number of latches.
    pub fn num_latches(&self) -> c_uint {
        unsafe { (*self.aiger).num_latches }
    }

    /// The number of outputs.
    pub fn num_outputs(&self) -> c_uint {
        unsafe { (*self.aiger).num_outputs }
    }

    /// The number of and gates.
    pub fn num_ands(&self) -> c_uint {
        unsafe { (*self.aiger).num_ands }
    }

    /// Adds an input to the aiger circuit with the given literal,
    /// which must be uncomplemented, and an optional name.
    pub fn add_input(&mut self, lit: c_uint, name: Option<&str>) {
        match name {
            Some(name) => {
                let c_name = CString::new(name).unwrap();
                let c_name_ptr = c_name.as_ptr();

                unsafe { aiger_add_input(self.aiger, lit, c_name_ptr) };
            }
            None => unsafe { aiger_add_input(self.aiger, lit, std::ptr::null()) },
        }
    }

    /// Adds an output to the aiger circuit with the given literal as next value
    /// and an optional name.
    pub fn add_output(&mut self, lit: c_uint, name: Option<&str>) {
        match name {
            Some(name) => {
                let c_name = CString::new(name).unwrap();
                let c_name_ptr = c_name.as_ptr();

                unsafe { aiger_add_output(self.aiger, lit, c_name_ptr) };
            }
            None => unsafe { aiger_add_output(self.aiger, lit, std::ptr::null()) },
        }
    }

    /// Adds an and gate to the aiger circuit with given `lhs` literal,
    /// which must not be complemented, and the given right-hand-sides as inputs.
    pub fn add_and(&mut self, lhs: c_uint, rhs0: c_uint, rhs1: c_uint) {
        unsafe { aiger_add_and(self.aiger, lhs, rhs0, rhs1) };
    }

    /// Add a latch to the aiger circuit with the given literal,
    /// which must be uncomplemented, the given next literal and an optional name.
    pub fn add_latch(&mut self, lit: c_uint, next: c_uint, name: Option<&str>) {
        match name {
            Some(name) => {
                let c_name = CString::new(name).unwrap();
                let c_name_ptr = c_name.as_ptr();

                unsafe { aiger_add_latch(self.aiger, lit, next, c_name_ptr) };
            }
            None => unsafe { aiger_add_input(self.aiger, lit, std::ptr::null()) },
        }
    }

    /// Sets the reset value of the latch with literal `lit` to `reset`.
    /// The value `reset` must be either constant true, constant false
    /// or equal to `lit`.
    pub fn add_reset(&mut self, lit: c_uint, reset: c_uint) {
        unsafe { aiger_add_reset(self.aiger, lit, reset) };
    }

    /// Writes the aiger circuit to the given writer in the given mode.
    ///
    /// # Errors
    ///
    /// If the writer returns an error during the write, then this error is returned.
    /// If a write operation does not write any bytes, then an I/O error of kind [`WriteZero`] is returned.
    ///
    /// [`WriteZero`]: std::io::ErrorKind::WriteZero
    pub fn write<W: Write>(&self, writer: W, mode: AigerMode) -> io::Result<()> {
        /// Wrapped writer to recover errors.
        struct WrappedWriter<W> {
            /// The wrapped writer.
            writer: W,
            /// The last error returned by the writer, if any.
            error: Option<io::Error>,
        }
        /// Rust version of aiger_put.
        extern "C" fn aiger_put<W>(character: c_char, data: *mut c_void) -> c_int
        where
            W: Write,
        {
            // convert to unsigned to avoid returning -1 except with EOF
            let character = character as u8;
            let wrapper = unsafe { &mut *(data as *mut WrappedWriter<W>) };
            match wrapper.writer.write(&[character]) {
                Ok(n) => {
                    if n == 1 {
                        character as c_int
                    } else {
                        EOF
                    }
                }
                Err(err) => {
                    wrapper.error = Some(err);
                    EOF
                }
            }
        }
        // create wrapper
        let mut wrapper = WrappedWriter {
            writer,
            error: None,
        };
        let data = &mut wrapper as *mut _;
        // call aiger write with address to writer
        let result = unsafe {
            aiger_write_generic(
                self.aiger,
                mode.as_aiger_mode(),
                data as *mut _,
                Some(aiger_put::<W>),
            )
        };
        // check result
        match wrapper.error {
            Some(err) => Err(err),
            None => {
                if result == 0 {
                    Err(io::Error::new(
                        io::ErrorKind::WriteZero,
                        "failure during aiger write".to_string(),
                    ))
                } else {
                    Ok(())
                }
            }
        }
    }

    /// Reads an aiger circuit from the given reader.
    ///
    /// # Errors
    ///
    /// If the reader returns an error during the read, then this error is returned.
    /// If the aiger circuit is malformed, then an I/O error of kind [`InvalidData`] with the
    /// underlying error message is returned.
    /// If the circuit creation fails, an I/O error of kind [`Other`] is returned.
    ///
    /// [`InvalidData`]: std::io::ErrorKind::InvalidData
    /// [`Other`]: std::io::ErrorKind::Other
    pub fn read<R: Read>(reader: R) -> io::Result<Self> {
        /// Wrapped reader to recover errors.
        struct WrappedReader<R> {
            /// The wrapped reader.
            reader: R,
            /// The last error returned by the reader, if any.
            error: Option<io::Error>,
        }
        /// Rust version of aiger_get.
        extern "C" fn aiger_get<R>(data: *mut c_void) -> c_int
        where
            R: Read,
        {
            let wrapper = unsafe { &mut *(data as *mut WrappedReader<R>) };
            let mut buf = [0];
            match wrapper.reader.read(&mut buf) {
                Ok(n) => {
                    if n == 1 {
                        buf[0] as c_int
                    } else {
                        EOF
                    }
                }
                Err(err) => {
                    wrapper.error = Some(err);
                    EOF
                }
            }
        }
        // initialize aiger
        let aiger = Self::new().map_err(|err| io::Error::new(io::ErrorKind::Other, err))?;
        // create wrapper
        let mut wrapper = WrappedReader {
            reader,
            error: None,
        };
        let data = &mut wrapper as *mut _;
        // call aiger read with address to reader
        let result =
            unsafe { aiger_read_generic(aiger.aiger, data as *mut _, Some(aiger_get::<R>)) };
        // check result
        match wrapper.error {
            Some(err) => Err(err),
            None => {
                if result.is_null() {
                    Ok(aiger)
                } else {
                    // extract error message
                    let c_str = unsafe { CStr::from_ptr(result) };
                    let error = c_str.to_string_lossy().into_owned();
                    Err(io::Error::new(io::ErrorKind::InvalidData, error))
                }
            }
        }
    }
}

/// Wrapped literal for safe use with [`AigerConstructor`].
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Literal(c_uint);

impl Literal {
    /// The constant false literal.
    pub const FALSE: Self = Self(AIGER_FALSE);
    /// The constant true literal.
    pub const TRUE: Self = Self(AIGER_TRUE);

    /// Returns the constant literal with given boolean value.
    pub fn from_bool(val: bool) -> Self {
        if val {
            Self::TRUE
        } else {
            Self::FALSE
        }
    }
}

impl std::ops::Not for Literal {
    type Output = Self;

    fn not(self) -> Self::Output {
        Self(aiger_not(self.0))
    }
}

/// Literal pair to use in a cache. Should always be normalized such that
/// the first literal is less or equal than the second.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
struct LiteralPair {
    /// First element of the pair.
    lit0: Literal,
    /// Second element of the pair.
    lit1: Literal,
}

/// A high-level constructor for an aiger circuit that can be used to
/// safely and incrementally add elements.
///
/// The constructor ensures that in the aiger circuit all
/// inputs appear before latches and all latches before and gates.
/// During construction, it hashes the inputs of existing and gates
/// and reuses them, as well as performing simplications for the inputs.
/// This ensures that an equivalent circuit is produced, but which may be smaller
/// than a direct construction.
///
/// To use the constructor, literals have to be constructed incrementally and
/// bound to variables, except for the constant true and false literals
/// which are available through [`Literal::TRUE`] and [`Literal::FALSE`].
///
/// # Examples
///
/// The following example constructs a simple circuit for storing the input value
/// `val` on an update by `upd` in the latch `latch`, with the output `cur`
/// for the current value of the latch:
///
/// ```
/// # use aiger::{AigerConstructor, Literal};
/// let mut constructor = AigerConstructor::new(2, 1).unwrap();
/// let upd = constructor.add_input("upd");
/// let val = constructor.add_input("val");
/// let latch = constructor.add_latch("latch");
/// let latch_next = constructor.add_ite(upd, val, latch);
/// constructor.set_latch_next(latch, latch_next);
/// constructor.set_latch_reset(latch, Literal::TRUE);
/// constructor.add_output("cur", latch);
/// let aiger = constructor.into_aiger();
/// ```
pub struct AigerConstructor {
    /// The aiger circuit that is currently being constructed.
    aig: Aiger,
    /// The count of the number of inputs that have already been added.
    cur_input: usize,
    /// The number of inputs that were specified.
    num_inputs: usize,
    /// The count of the number of latches that have already been added.
    cur_latch: usize,
    /// The number of latches that were specified.
    num_latches: usize,
    /// The names of latches that were already added.
    latches: Vec<String>,
    /// The count of and gates that were added.
    cur_and: usize,
    /// The cache of already added and gates, mapping their inputs to the uncomplemented literal.
    and_cache: HashMap<LiteralPair, Literal>,
}

impl AigerConstructor {
    /// Creates a new aiger circuit constructor with a pre-specified number of inputs and latches.
    ///
    /// # Errors
    ///
    /// Returns an error if the initialization of the aiger circuit fails.
    pub fn new(num_inputs: usize, num_latches: usize) -> Result<Self, String> {
        let aig = Aiger::new()?;
        Ok(Self {
            aig,
            cur_input: 0,
            num_inputs,
            cur_latch: 0,
            num_latches,
            latches: Vec::with_capacity(num_latches),
            cur_and: 0,
            and_cache: HashMap::new(),
        })
    }

    /// Adds an and gate to the circuit with `lhs` and `rhs` as inputs,
    /// and returns the literal for the and gate.
    ///
    /// May return a constant literal or an already existing literal
    /// if a simplification rule is used or the literal is in the cache.
    pub fn add_and(&mut self, lhs: Literal, rhs: Literal) -> Literal {
        if lhs == rhs {
            lhs
        } else if lhs == !rhs || lhs == Literal::FALSE || rhs == Literal::FALSE {
            Literal::FALSE
        } else if lhs == Literal::TRUE {
            rhs
        } else if rhs == Literal::TRUE {
            lhs
        } else {
            // normalize for cache
            let (lhs, rhs) = (std::cmp::min(lhs, rhs), std::cmp::max(lhs, rhs));
            let pair = LiteralPair {
                lit0: lhs,
                lit1: rhs,
            };
            match self.and_cache.entry(pair) {
                Entry::Occupied(entry) => *entry.get(),
                Entry::Vacant(entry) => {
                    let lit = Literal(aiger_var2lit(
                        (1 + self.num_inputs + self.num_latches + self.cur_and) as c_uint,
                    ));
                    self.aig.add_and(lit.0, lhs.0, rhs.0);
                    self.cur_and += 1;
                    entry.insert(lit);
                    lit
                }
            }
        }
    }

    /// Adds an or gate to the circuit with `lhs` and `rhs` as inputs,
    /// and returns the literal for the or gate.
    ///
    /// This function simply forwards to [`add_and`](AigerConstructor::add_and) and uses the same
    /// simplification and cache rules.
    pub fn add_or(&mut self, lhs: Literal, rhs: Literal) -> Literal {
        !self.add_and(!lhs, !rhs)
    }

    /// Adds gates for an "if-then-else" construct, and returns the literal for the output.
    ///
    /// This function performs additional simplifations over a direct construction with
    /// and gates, but falls back to such construction otherwise. In that case, it
    /// adds three and gates where the output corresponds to `(¬lit ∨ then_lit) ∧ (lit ∨ else_lit)`.
    pub fn add_ite(&mut self, lit: Literal, then_lit: Literal, else_lit: Literal) -> Literal {
        if lit == Literal::TRUE || then_lit == else_lit {
            then_lit
        } else if lit == Literal::FALSE {
            else_lit
        } else if then_lit == Literal::TRUE || lit == then_lit {
            self.add_or(lit, else_lit)
        } else if then_lit == Literal::FALSE || !lit == then_lit {
            self.add_and(!lit, else_lit)
        } else if else_lit == Literal::TRUE || !lit == else_lit {
            self.add_or(!lit, then_lit)
        } else if else_lit == Literal::FALSE || lit == else_lit {
            self.add_and(lit, then_lit)
        } else {
            let then_or_lit = self.add_or(!lit, then_lit);
            let else_or_lit = self.add_or(lit, else_lit);
            self.add_and(then_or_lit, else_or_lit)
        }
    }

    /// Adds an input with the given name to the circuit, and returns the literal for the input.
    ///
    /// # Panics
    ///
    /// Panics if already a number of inputs as specified during the initialization were added.
    pub fn add_input(&mut self, name: &str) -> Literal {
        assert!(self.cur_input < self.num_inputs);
        let lit = Literal(aiger_var2lit((1 + self.cur_input) as c_uint));
        self.aig.add_input(lit.0, Some(name));
        self.cur_input += 1;
        lit
    }

    /// Adds a latch with the given name to the circuit, and returns the literal for the latch.
    ///
    /// # Panics
    ///
    /// Panics if already a number of latches as specified during the initialization were added.
    pub fn add_latch(&mut self, name: &str) -> Literal {
        assert!(self.cur_latch < self.num_latches);
        self.latches.push(name.to_string());
        let lit = Literal(aiger_var2lit(
            (1 + self.num_inputs + self.cur_latch) as c_uint,
        ));
        self.cur_latch += 1;
        lit
    }

    /// Adds an output wtih the given name and the next value given by `lit` to the circuit.
    pub fn add_output(&mut self, name: &str, lit: Literal) {
        self.aig.add_output(lit.0, Some(name));
    }

    /// Returns the index of a latch literal in the internal list of names.
    ///
    /// # Panics
    ///
    /// Panics if the given literal is not a latch.
    fn latch_index(&self, latch: Literal) -> usize {
        let var = aiger_lit2var(latch.0) as usize;
        if !(var > self.num_inputs && var <= 1 + self.num_inputs + self.cur_latch) {
            panic!("Literal is not a latch: {}", latch.0)
        }
        var - (1 + self.num_inputs)
    }

    /// Sets the next value of a latch `next`.
    ///
    /// # Panics
    ///
    /// Panics if the given literal is not a latch.
    pub fn set_latch_next(&mut self, latch: Literal, next: Literal) {
        let index = self.latch_index(latch);
        let name = &self.latches[index];
        self.aig.add_latch(latch.0, next.0, Some(name));
    }

    /// Sets the next reset value of a latch to `reset`.
    ///
    /// # Panics
    ///
    /// Panics if the given literal is not a latch or the reset value is not constant true or false.
    pub fn set_latch_reset(&mut self, latch: Literal, reset: Literal) {
        assert!(reset == Literal::TRUE || reset == Literal::FALSE);
        let _ = self.latch_index(latch);
        self.aig.add_reset(latch.0, reset.0);
    }

    /// Consumes this constructor and returns the aiger circuit constructed by it.
    ///
    /// # Panics
    ///
    /// Panics if not all latches and inputs as initially specified were added,
    /// or not all latches have been assigned a next value.
    pub fn into_aiger(self) -> Aiger {
        assert!(self.cur_input == self.num_inputs);
        assert!(self.cur_latch == self.num_latches);
        assert!(self.latches.len() == self.num_latches);
        self.aig
    }
}

impl fmt::Display for Aiger {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut bytes = Vec::new();
        self.write(&mut bytes, AigerMode::Ascii).unwrap();
        write!(f, "{}", String::from_utf8(bytes).unwrap())
    }
}

impl fmt::Display for AigerConstructor {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.aig)
    }
}

/// Tests for the aiger wrapper and the aiger constructor.
#[cfg(test)]
mod tests {
    use super::*;

    /// Test that simplifications by the aiger constructor work.
    #[test]
    fn test_aiger_simplifications() {
        let mut aig = AigerConstructor::new(3, 0).unwrap();
        let x = aig.add_input("x");
        let y = aig.add_input("y");
        let z = aig.add_input("z");

        // test that all inputs and constants are distinct
        assert_ne!(Literal::TRUE, Literal::FALSE, "⊤ ≠ ⊥");
        assert_ne!(x, Literal::TRUE, "x ≠ ⊤");
        assert_ne!(y, Literal::TRUE, "y ≠ ⊤");
        assert_ne!(z, Literal::TRUE, "z ≠ ⊤");
        assert_ne!(x, Literal::FALSE, "x ≠ ⊥");
        assert_ne!(y, Literal::FALSE, "y ≠ ⊥");
        assert_ne!(z, Literal::FALSE, "z ≠ ⊥");

        assert_ne!(x, y, "x ≠ y");
        assert_ne!(x, z, "x ≠ z");
        assert_ne!(y, z, "y ≠ z");
        assert_ne!(x, !x, "x ≠ ¬x");
        assert_ne!(x, !y, "x ≠ ¬y");
        assert_ne!(x, !z, "x ≠ ¬z");
        assert_ne!(y, !x, "y ≠ ¬x");
        assert_ne!(y, !y, "y ≠ ¬y");
        assert_ne!(y, !z, "y ≠ ¬z");
        assert_ne!(z, !y, "z ≠ ¬x");
        assert_ne!(z, !z, "z ≠ ¬y");
        assert_ne!(z, !z, "z ≠ ¬z");

        // test negation
        assert_eq!(!Literal::TRUE, Literal::FALSE, "¬⊤ = ⊥");
        assert_eq!(!Literal::FALSE, Literal::TRUE, "¬⊥ = ⊤");
        assert_eq!(x, !!x, "x = ¬¬x");
        assert_eq!(y, !!y, "y = ¬¬y");
        assert_eq!(z, !!z, "y = ¬¬y");

        // test and + or simplifications
        assert_eq!(aig.add_and(x, x), x, "x ∧ x = x");
        assert_eq!(aig.add_or(x, x), x, "x ∨ x = x");

        assert_eq!(aig.add_and(x, !x), Literal::FALSE, "x ∧ ¬x = ⊥");
        assert_eq!(aig.add_and(!x, x), Literal::FALSE, "¬x ∧ x = ⊥");

        assert_eq!(aig.add_and(x, Literal::TRUE), x, "x ∧ ⊤ = x");
        assert_eq!(aig.add_and(Literal::TRUE, x), x, "⊤ ∧ x = x");
        assert_eq!(aig.add_and(x, Literal::FALSE), Literal::FALSE, "x ∧ ⊥ = ⊥");
        assert_eq!(aig.add_and(Literal::FALSE, x), Literal::FALSE, "⊥ ∧ x = ⊥");

        assert_eq!(aig.add_or(x, Literal::FALSE), x, "x ∨ ⊥ = x");
        assert_eq!(aig.add_or(Literal::FALSE, x), x, "⊥ ∨ x = x");
        assert_eq!(aig.add_or(x, Literal::TRUE), Literal::TRUE, "x ∨ ⊤ = ⊤");
        assert_eq!(aig.add_or(Literal::TRUE, x), Literal::TRUE, "⊤ ∨ x = ⊤");

        // test and cache
        let yz = aig.add_and(y, z);
        assert_ne!(yz, Literal::TRUE, "y ∧ z ≠ ⊤");
        assert_ne!(yz, Literal::FALSE, "y ∧ z ≠ ⊥");
        assert_ne!(yz, y, "y ∧ z ≠ y");
        assert_ne!(yz, !y, "y ∧ z ≠ ¬y");
        assert_ne!(yz, z, "y ∧ z ≠ z");
        assert_ne!(yz, !z, "y ∧ z ≠ ¬z");

        assert_eq!(aig.add_and(y, z), yz, "y ∧ z = y ∧ z (cache)");
        assert_eq!(aig.add_and(z, y), yz, "z ∧ y = y ∧ z (cache)");

        // test if-then-else
        assert_eq!(aig.add_ite(Literal::TRUE, y, z), y, "ite(⊤, y, z) = y");
        assert_eq!(aig.add_ite(Literal::FALSE, y, z), z, "ite(⊥, y, z) = z");

        assert_eq!(
            aig.add_ite(x, Literal::FALSE, z),
            aig.add_and(!x, z),
            "ite(x, ⊥, z) = ¬x ∧ z"
        );
        assert_eq!(
            aig.add_ite(x, y, Literal::FALSE),
            aig.add_and(x, y),
            "ite(x, y, ⊥) = x ∧ y"
        );
        assert_eq!(
            aig.add_ite(x, Literal::TRUE, z),
            aig.add_or(x, z),
            "ite(x, ⊤, z) = x ∨ z"
        );
        assert_eq!(
            aig.add_ite(x, y, Literal::TRUE),
            aig.add_or(!x, y),
            "ite(x, y, ⊤) = ¬x ∨ y"
        );

        assert_eq!(aig.add_ite(x, y, y), y, "ite(x, y, y) = y");

        assert_eq!(
            aig.add_ite(x, y, x),
            aig.add_and(x, y),
            "ite(x, y, x) = x ∧ y"
        );
        assert_eq!(
            aig.add_ite(x, x, z),
            aig.add_or(x, z),
            "ite(x, x, z) = x ∨ z"
        );
        assert_eq!(
            aig.add_ite(x, !x, z),
            aig.add_and(!x, z),
            "ite(x, ¬x, z) = ¬x ∧ z"
        );
        assert_eq!(
            aig.add_ite(x, y, !x),
            aig.add_or(!x, y),
            "ite(x, y, ¬x) = ¬x ∨ y"
        );

        // test for new if-then-else node (currently implementation specific)
        let nxy = aig.add_or(!x, y);
        let xz = aig.add_or(x, z);
        assert_eq!(
            aig.add_ite(x, y, z),
            aig.add_and(nxy, xz),
            "ite(x, y, z) = (¬x ∨ y) ∧ (x ∨ z)"
        );
        // the following does not yet work: ite(x, y, z) = (x ∧ y) ∨ (¬x ∧ ¬z)"
    }

    /// Test reading an aiger circuit, including tests for errors.
    #[test]
    fn test_aiger_read() {
        // valid read
        let aig_valid = "aag 0 0 0 0 0\n".as_bytes();
        let result = Aiger::read(aig_valid);
        assert!(result.is_ok());

        // read succeeds, but invalid data
        let aig_invalid = "not an aiger\n".as_bytes();
        let result = Aiger::read(aig_invalid);
        assert!(result.is_err());
        assert_eq!(result.unwrap_err().kind(), io::ErrorKind::InvalidData);

        // read fails
        let read_err = std::fs::File::open(".").unwrap();
        let result = Aiger::read(read_err);
        assert!(result.is_err());
        assert_ne!(result.unwrap_err().kind(), io::ErrorKind::InvalidData);
    }

    /// Test writing an aiger circuit, including tests for errors.
    #[test]
    fn test_aiger_write() {
        // valid write
        let mut constructor = AigerConstructor::new(2, 1).unwrap();
        let upd = constructor.add_input("upd");
        let val = constructor.add_input("val");
        let latch = constructor.add_latch("latch");

        let latch_next = constructor.add_ite(upd, val, latch);
        constructor.set_latch_next(latch, latch_next);
        constructor.set_latch_reset(latch, Literal::TRUE);
        constructor.add_output("cur", latch);

        let aig = constructor.into_aiger();
        let mut aig_vec = Vec::new();
        let result = aig.write(&mut aig_vec, AigerMode::Ascii);
        assert!(result.is_ok());
        let aig_str = String::from_utf8(aig_vec).unwrap();

        assert_eq!(
            aig_str,
            "\
            aag 6 2 1 1 3\n\
            2\n\
            4\n\
            6 12 1\n\
            6\n\
            8 2 5\n\
            10 3 7\n\
            12 9 11\n\
            i0 upd\n\
            i1 val\n\
            l0 latch\n\
            o0 cur\n\
        "
        );

        // write fails
        let write_err = std::fs::File::open(".").unwrap();
        let result = aig.write(write_err, AigerMode::Ascii);
        assert!(result.is_err());
        assert_ne!(result.unwrap_err().kind(), io::ErrorKind::InvalidData);
    }
}
