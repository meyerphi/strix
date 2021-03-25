#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]
#![allow(clippy::unreadable_literal)]
#![allow(clippy::redundant_static_lifetimes)]
#![allow(clippy::upper_case_acronyms)]

// include bindings generated by build.rs with bindgen
include!(concat!(env!("OUT_DIR"), "/cudd_bindings.rs"));

/// Complements a node pointer
pub(super) fn Cudd_Not(node: *mut DdNode) -> *mut DdNode {
    ((node as usize) ^ 1_usize) as *mut DdNode
}

/// Returns the regular version of a node pointer.
pub(super) fn Cudd_Regular(node: *mut DdNode) -> *mut DdNode {
    ((node as usize) & !1_usize) as *mut DdNode
}

/// Returns the complemented version of a node pointer.
pub(super) fn Cudd_Complement(node: *mut DdNode) -> *mut DdNode {
    ((node as usize) | !1_usize) as *mut DdNode
}

/// Returns true if a node pointer is complemented.
pub(super) fn Cudd_IsComplement(node: *mut DdNode) -> bool {
    ((node as usize) & 1_usize) != 0
}
