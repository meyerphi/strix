mod bindings;

use std::collections::hash_map::Entry;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::fmt;
use std::io::{Read, Write};
use std::os::raw::{c_char, c_int, c_uint, c_void};

use bindings::*;

// low level aiger interface

pub type AigerRaw = aiger;

pub const fn aiger_sign(lit: c_uint) -> c_uint {
    lit & 1
}
pub const fn aiger_strip(lit: c_uint) -> c_uint {
    lit & !1
}
pub const fn aiger_not(lit: c_uint) -> c_uint {
    lit ^ 1
}
pub const fn aiger_var2lit(var: c_uint) -> c_uint {
    var << 1
}
pub const fn aiger_lit2var(lit: c_uint) -> c_uint {
    lit >> 1
}

pub struct Aiger {
    aiger: *mut AigerRaw,
}

impl Drop for Aiger {
    fn drop(&mut self) {
        unsafe { aiger_reset(self.aiger) };
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum AigerMode {
    Ascii,
    Binary,
}

impl AigerMode {
    fn as_aiger_mode(&self) -> aiger_mode {
        match self {
            AigerMode::Ascii => aiger_mode_aiger_ascii_mode,
            AigerMode::Binary => aiger_mode_aiger_binary_mode,
        }
    }
}

impl Aiger {
    pub fn new() -> Result<Aiger, &'static str> {
        let aiger = unsafe { aiger_init() };
        if aiger.is_null() {
            Err("Failed to initialize aiger")
        } else {
            Ok(Aiger { aiger })
        }
    }

    /// # Safety
    ///
    /// aiger must point to a valid aiger struct.
    pub unsafe fn from_raw(aiger: *mut AigerRaw) -> Aiger {
        Aiger { aiger }
    }

    /// # Safety
    ///
    /// self must not be dropped while the pointer is used
    pub unsafe fn raw_ptr(&self) -> *mut AigerRaw {
        self.aiger
    }

    pub fn maxvar(&self) -> c_uint {
        unsafe { (*self.aiger).maxvar }
    }

    pub fn num_inputs(&self) -> c_uint {
        unsafe { (*self.aiger).num_inputs }
    }

    pub fn num_latches(&self) -> c_uint {
        unsafe { (*self.aiger).num_latches }
    }

    pub fn num_outputs(&self) -> c_uint {
        unsafe { (*self.aiger).num_outputs }
    }

    pub fn num_ands(&self) -> c_uint {
        unsafe { (*self.aiger).num_ands }
    }

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

    pub fn add_and(&mut self, lhs: c_uint, rhs0: c_uint, rhs1: c_uint) {
        unsafe { aiger_add_and(self.aiger, lhs, rhs0, rhs1) };
    }

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

    pub fn add_reset(&mut self, lit: c_uint, reset: c_uint) {
        unsafe { aiger_add_reset(self.aiger, lit, reset) };
    }

    pub fn write<W: Write>(&self, writer: W, mode: AigerMode) {
        // rust version of aiger_put
        extern "C" fn aiger_put<W>(character: c_char, data: *mut c_void) -> c_int
        where
            W: Write,
        {
            let writer = unsafe { &mut *(data as *mut W) };
            match writer.write(&[character as u8]) {
                Ok(n) => {
                    if n == 1 {
                        character as c_int
                    } else {
                        EOF
                    }
                }
                Err(_) => EOF,
            }
        }
        // place writer on the heap
        let data = Box::into_raw(Box::new(writer));
        // call aiger write with address to writer
        unsafe {
            aiger_write_generic(
                self.aiger,
                mode.as_aiger_mode(),
                data as *mut _,
                Some(aiger_put::<W>),
            )
        };
        // recover writer from heap to drop it
        unsafe { Box::from_raw(data as *mut W) };
    }

    pub fn read<R: Read>(reader: R) -> Result<Aiger, String> {
        let aiger = Aiger::new()?;

        // rust version of aiger_get
        extern "C" fn aiger_get<R>(data: *mut c_void) -> c_int
        where
            R: Read,
        {
            let reader = unsafe { &mut *(data as *mut R) };
            let mut buf = [0];
            match reader.read(&mut buf) {
                Ok(n) => {
                    if n == 1 {
                        buf[0] as c_int
                    } else {
                        EOF
                    }
                }
                Err(_) => EOF,
            }
        }
        // place reader on the heap
        let data = Box::into_raw(Box::new(reader));
        // call aiger read with address to reader
        let result =
            unsafe { aiger_read_generic(aiger.aiger, data as *mut _, Some(aiger_get::<R>)) };
        // recover reader from heap to drop it
        unsafe { Box::from_raw(data as *mut R) };
        // check result
        if result.is_null() {
            Ok(aiger)
        } else {
            // extract error message
            let c_str = unsafe { CStr::from_ptr(result) };
            let error = c_str.to_string_lossy().into_owned();
            Err(error)
        }
    }
}

// high level aiger interface and builder

#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Literal(c_uint);

impl Literal {
    pub const FALSE: Literal = Literal(aiger_false);
    pub const TRUE: Literal = Literal(aiger_true);

    pub fn from_bool(val: bool) -> Literal {
        if val {
            Literal::TRUE
        } else {
            Literal::FALSE
        }
    }
}

impl std::ops::Not for Literal {
    type Output = Literal;

    fn not(self) -> Self::Output {
        Literal(aiger_not(self.0))
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
struct LiteralPair {
    lit0: Literal,
    lit1: Literal,
}

pub struct AigerConstructor {
    aig: Aiger,
    cur_input: usize,
    num_inputs: usize,
    cur_latch: usize,
    num_latches: usize,
    latches: Vec<String>,
    cur_and: usize,
    and_cache: HashMap<LiteralPair, Literal>,
}

impl AigerConstructor {
    pub fn new(num_inputs: usize, num_latches: usize) -> Result<AigerConstructor, String> {
        let aig = Aiger::new()?;
        Ok(AigerConstructor {
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

    pub fn add_or(&mut self, lhs: Literal, rhs: Literal) -> Literal {
        !self.add_and(!lhs, !rhs)
    }

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

    pub fn add_input(&mut self, name: &str) -> Literal {
        assert!(self.cur_input < self.num_inputs);
        let lit = Literal(aiger_var2lit((1 + self.cur_input) as c_uint));
        self.aig.add_input(lit.0, Some(name));
        self.cur_input += 1;
        lit
    }

    pub fn add_latch(&mut self, name: &str) -> Literal {
        assert!(self.cur_latch < self.num_latches);
        self.latches.push(name.to_string());
        let lit = Literal(aiger_var2lit(
            (1 + self.num_inputs + self.cur_latch) as c_uint,
        ));
        self.cur_latch += 1;
        lit
    }

    pub fn add_output(&mut self, name: &str, lit: Literal) {
        self.aig.add_output(lit.0, Some(name));
    }

    fn latch_index(&self, latch: Literal) -> usize {
        let var = aiger_lit2var(latch.0) as usize;
        if !(var > self.num_inputs && var <= 1 + self.num_inputs + self.cur_latch) {
            panic!("Literal is not a latch: {}", latch.0)
        }
        var - (1 + self.num_inputs)
    }

    pub fn set_latch_next(&mut self, latch: Literal, next: Literal) {
        let index = self.latch_index(latch);
        let name = &self.latches[index];
        self.aig.add_latch(latch.0, next.0, Some(name));
    }

    pub fn set_latch_reset(&mut self, latch: Literal, reset: Literal) {
        assert!(reset == Literal::TRUE || reset == Literal::FALSE);
        self.aig.add_reset(latch.0, reset.0);
    }

    pub fn write<W: Write>(&self, writer: W, mode: AigerMode) {
        self.aig.write(writer, mode);
    }

    pub fn into_aiger(self) -> Aiger {
        self.aig
    }
}

impl fmt::Display for Aiger {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut bytes = Vec::new();
        self.write(&mut bytes, AigerMode::Ascii);
        write!(f, "{}", String::from_utf8(bytes).unwrap())
    }
}

impl fmt::Display for AigerConstructor {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.aig)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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

    #[test]
    fn test_aiger_write() {
        let mut aig = AigerConstructor::new(2, 1).unwrap();
        let upd = aig.add_input("upd");
        let val = aig.add_input("val");
        let latch = aig.add_latch("latch");

        let latch_next = aig.add_ite(upd, val, latch);
        aig.set_latch_next(latch, latch_next);
        aig.set_latch_reset(latch, Literal::TRUE);
        aig.add_output("cur", latch);

        let aig_str = format!("{}", aig);

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
    }
}
