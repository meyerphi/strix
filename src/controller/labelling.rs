use std::collections::HashMap;
use std::fmt;
use std::hash::Hash;
use std::ops::Index;

use owl::{automaton::MaxEvenDPA, StateIndex};

pub type LabelInnerValue = u32;
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum LabelValue {
    DontCare,
    Value(LabelInnerValue),
}

impl LabelValue {
    pub fn num_bits(&self) -> u32 {
        match self {
            LabelValue::DontCare => 0,
            LabelValue::Value(val) => {
                (std::mem::size_of::<LabelInnerValue>() as u32) * 8 - val.leading_zeros()
            }
        }
    }

    pub fn bit(&self, index: u32) -> bool {
        match self {
            LabelValue::DontCare => false,
            LabelValue::Value(val) => val & (1 << index) != 0,
        }
    }
}

impl fmt::Display for LabelValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LabelValue::DontCare => write!(f, "-"),
            LabelValue::Value(val) => write!(f, "{}", val),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StructuredLabel {
    label: Vec<LabelValue>,
}

impl StructuredLabel {
    fn new(label: Vec<LabelValue>) -> Self {
        StructuredLabel { label }
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

pub trait Labelling<L> {
    /// Prepare the labels for the state indices in the given iterator.
    fn prepare_labels<'a, I: Iterator<Item = &'a L>>(&'a mut self, label_iter: I)
    where
        L: 'a;

    /// Return the label for the given index.
    fn get_label(&self, label: &L) -> StructuredLabel;
}

pub struct SimpleLabelling<L> {
    mapping: HashMap<L, LabelValue>,
}

impl<L> Default for SimpleLabelling<L> {
    fn default() -> Self {
        SimpleLabelling {
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
                .insert(label.clone(), LabelValue::Value(val as u32));
        }
    }

    fn get_label(&self, index: &L) -> StructuredLabel {
        StructuredLabel::new(vec![self.mapping[index]])
    }
}

pub struct AutomatonLabelling<'a, A> {
    automaton: &'a A,
    width: usize,
}

impl<'a, A> AutomatonLabelling<'a, A> {
    pub fn new(automaton: &'a A) -> Self {
        AutomatonLabelling {
            automaton,
            width: 1,
        }
    }
}

impl<'a, A: MaxEvenDPA> Labelling<StateIndex> for AutomatonLabelling<'a, A> {
    fn prepare_labels<'b, I: Iterator<Item = &'b StateIndex>>(&'b mut self, _: I) {}

    fn get_label(&self, index: &StateIndex) -> StructuredLabel {
        let index = *index;
        if index == StateIndex::TOP || index == StateIndex::BOTTOM {
            StructuredLabel::new(vec![LabelValue::DontCare; self.width])
        } else {
            StructuredLabel::new(
                self.automaton
                    .decompose(index)
                    .into_iter()
                    .map(|v| {
                        if v < 0 {
                            LabelValue::DontCare
                        } else {
                            LabelValue::Value(v as u32)
                        }
                    })
                    .collect(),
            )
        }
    }
}
