use std::collections::HashMap;
use std::fmt;
use std::hash::Hash;
use std::iter;
use std::ops::Index;

use owl::automaton::{MaxEvenDPA, StateIndex};
use owl::tree::TreeIndex;

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct AutomatonTreeLabel {
    automaton_state: StateIndex,
    tree_index: TreeIndex,
}

impl std::fmt::Display for AutomatonTreeLabel {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "({}, {})", self.automaton_state, self.tree_index)
    }
}

impl AutomatonTreeLabel {
    pub(crate) const fn new(automaton_state: StateIndex, tree_index: TreeIndex) -> Self {
        Self {
            automaton_state,
            tree_index,
        }
    }

    pub const fn automaton_state(&self) -> StateIndex {
        self.automaton_state
    }

    pub const fn tree_index(&self) -> TreeIndex {
        self.tree_index
    }
}

pub type LabelInnerValue = u64;
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum LabelValue {
    DontCare,
    Value(LabelInnerValue),
}

impl LabelValue {
    pub(crate) const fn num_bits(self) -> u32 {
        match self {
            Self::DontCare => 0,
            Self::Value(val) => {
                (std::mem::size_of::<LabelInnerValue>() as u32) * 8 - val.leading_zeros()
            }
        }
    }

    pub(crate) const fn bit(self, index: u32) -> bool {
        match self {
            Self::DontCare => false,
            Self::Value(val) => val & (1 << index) != 0,
        }
    }
}

impl fmt::Display for LabelValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::DontCare => write!(f, "-"),
            Self::Value(val) => write!(f, "{}", val),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StructuredLabel {
    label: Vec<LabelValue>,
}

impl StructuredLabel {
    fn new(label: Vec<LabelValue>) -> Self {
        Self { label }
    }

    pub fn components(&self) -> usize {
        self.label.len()
    }

    pub fn iter(&self) -> impl Iterator<Item = &LabelValue> {
        self.label.iter()
    }
}

impl Index<usize> for StructuredLabel {
    type Output = LabelValue;

    fn index(&self, index: usize) -> &Self::Output {
        &self.label[index]
    }
}

impl fmt::Display for StructuredLabel {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[")?;
        for (i, v) in self.label.iter().enumerate() {
            if i > 0 {
                write!(f, ",")?;
            }
            write!(f, "{}", v)?;
        }
        write!(f, "]")?;
        Ok(())
    }
}

pub(crate) trait Labelling<L> {
    /// Prepare the labels for the state indices in the given iterator.
    fn prepare_labels<'a, I: Iterator<Item = &'a L>>(&'a mut self, label_iter: I)
    where
        L: 'a;

    /// Return the label for the given index.
    fn get_label(&self, label: &L) -> StructuredLabel;
}

pub(crate) struct SimpleLabelling<L> {
    mapping: HashMap<L, LabelValue>,
}

impl<L> Default for SimpleLabelling<L> {
    fn default() -> Self {
        Self {
            mapping: HashMap::new(),
        }
    }
}

impl<L: Clone + Eq + Hash> Labelling<L> for SimpleLabelling<L> {
    fn prepare_labels<'a, I: Iterator<Item = &'a L>>(&'a mut self, label_iter: I)
    where
        L: 'a,
    {
        for (val, label) in label_iter.enumerate() {
            self.mapping
                .insert(label.clone(), LabelValue::Value(val as LabelInnerValue));
        }
    }

    fn get_label(&self, index: &L) -> StructuredLabel {
        StructuredLabel::new(vec![self.mapping[index]])
    }
}

pub(crate) struct AutomatonLabelling<'a, A> {
    label_count: HashMap<StateIndex, usize>,
    automaton: &'a A,
    width: usize,
    local_width: usize,
}

impl<'a, A> AutomatonLabelling<'a, A> {
    pub(crate) fn new(automaton: &'a A) -> Self {
        AutomatonLabelling {
            label_count: HashMap::new(),
            automaton,
            width: 0,
            local_width: 1,
        }
    }
}

impl<'a, A: MaxEvenDPA> AutomatonLabelling<'a, A> {
    fn get_label(&self, states: &[StateIndex]) -> StructuredLabel {
        let mut values = Vec::new();
        for &index in states {
            if index == StateIndex::TOP || index == StateIndex::BOTTOM {
                values.extend(iter::repeat(LabelValue::DontCare).take(self.local_width));
            } else {
                values.extend(self.automaton.decompose(index).iter().map(|&val| {
                    if val < 0 {
                        LabelValue::DontCare
                    } else {
                        LabelValue::Value(val as LabelInnerValue)
                    }
                }));
            }
        }
        // extend to common width
        values.extend(
            iter::repeat(LabelValue::DontCare).take((self.width * self.local_width) - values.len()),
        );
        StructuredLabel::new(values)
    }
}

impl<'a, A: MaxEvenDPA> Labelling<StateIndex> for AutomatonLabelling<'a, A> {
    fn prepare_labels<'b, I: Iterator<Item = &'b StateIndex>>(&'b mut self, index_iter: I) {
        self.width = 1;
        for &index in index_iter {
            *self.label_count.entry(index).or_insert(0) += 1;
        }
        for &count in self.label_count.values() {
            // indices are singletons and thus need to be unique
            assert!(count == 1);
        }
    }

    fn get_label(&self, index: &StateIndex) -> StructuredLabel {
        self.get_label(&[*index])
    }
}

impl<'a, A: MaxEvenDPA> Labelling<Vec<StateIndex>> for AutomatonLabelling<'a, A> {
    fn prepare_labels<'b, I: Iterator<Item = &'b Vec<StateIndex>>>(&'b mut self, index_iter: I) {
        for states in index_iter {
            self.width = std::cmp::max(self.width, states.len());
            for &index in states.iter() {
                *self.label_count.entry(index).or_insert(0) += 1;
            }
        }
        // TODO: add label compression by choosing unique subset
    }

    fn get_label(&self, indices: &Vec<StateIndex>) -> StructuredLabel {
        let mut sorted_indices = indices.clone();
        sorted_indices.sort();
        self.get_label(&sorted_indices)
    }
}
