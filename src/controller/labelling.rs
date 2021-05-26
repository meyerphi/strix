//! Labels for parity games and machines based on automata.

use std::cmp::Ordering;
use std::collections::HashMap;
use std::fmt;
use std::hash::Hash;
use std::iter;
use std::ops::Index;

use log::debug;

use owl::automaton::{MaxEvenDpa, StateIndex, ZielonkaNormalFormState};
use owl::tree::TreeIndex;

/// A label referencing a state in an automaton
/// and a node in the edge tree of that state.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct AutomatonTreeLabel {
    /// The index of the state of the automaton.
    automaton_state: StateIndex,
    /// The index of the node of the edge tree.
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

    /// Returns the index of the state of the automaton in this label.
    pub const fn automaton_state(&self) -> StateIndex {
        self.automaton_state
    }

    /// Returns the index of the node of the edge tree in this label.
    pub const fn tree_index(&self) -> TreeIndex {
        self.tree_index
    }
}

/// The type for the concrete value of a component in a [`StructuredLabel`].
pub type LabelInnerValue = u64;
/// The value of of a component in a [`StructuredLabel`].
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LabelValue {
    /// A don't care value, which may be instantiated with any value.
    DontCare,
    /// A concrete value with.
    Value(LabelInnerValue),
}

impl Ord for LabelValue {
    fn cmp(&self, other: &Self) -> Ordering {
        match (&self, other) {
            (LabelValue::DontCare, LabelValue::DontCare) => Ordering::Equal,
            (LabelValue::DontCare, LabelValue::Value(_)) => Ordering::Less,
            (LabelValue::Value(_), LabelValue::DontCare) => Ordering::Greater,
            (LabelValue::Value(v1), LabelValue::Value(v2)) => v1.cmp(v2),
        }
    }
}

impl PartialOrd for LabelValue {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
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

    pub(crate) const fn is_value(self) -> bool {
        matches!(self, LabelValue::Value(_))
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

/// A structured label consisting of a list of label values,
/// called the components of the structured label.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StructuredLabel {
    label: Vec<LabelValue>,
}

impl StructuredLabel {
    fn new(label: Vec<LabelValue>) -> Self {
        Self { label }
    }

    /// Returns the number of components in this structured label.
    pub fn components(&self) -> usize {
        self.label.len()
    }

    /// Returns an iterator over the values of the components in this
    /// structured label.
    pub fn iter(&self) -> impl Iterator<Item = &LabelValue> {
        self.label.iter()
    }

    /// Returns an iterator that allow modifying each value in this
    /// structured label.
    pub fn iter_mut(&mut self) -> impl Iterator<Item = &mut LabelValue> {
        self.label.iter_mut()
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
    automaton: &'a A,
    feature_map: HashMap<StateIndex, StructuredLabel>,
}

impl<'a, A> AutomatonLabelling<'a, A> {
    pub(crate) fn new(automaton: &'a A) -> Self {
        AutomatonLabelling {
            automaton,
            feature_map: HashMap::new(),
        }
    }
}

impl<'a, A: MaxEvenDpa> AutomatonLabelling<'a, A> {
    fn get_label(&self, states: &[StateIndex]) -> StructuredLabel {
        let mut values = Vec::new();
        for index in states {
            values.extend(self.feature_map[index].iter());
        }
        StructuredLabel::new(values)
    }
}

impl<'a, A: MaxEvenDpa> Labelling<StateIndex> for AutomatonLabelling<'a, A> {
    fn prepare_labels<'b, I: Iterator<Item = &'b StateIndex>>(&'b mut self, iter: I) {
        let features = self.automaton.extract_features(iter);
        self.feature_map = zielonka_normal_form_to_labelling(&features);
    }

    fn get_label(&self, index: &StateIndex) -> StructuredLabel {
        self.feature_map[index].clone()
    }
}

impl<'a, A: MaxEvenDpa> Labelling<Vec<StateIndex>> for AutomatonLabelling<'a, A> {
    fn prepare_labels<'b, I: Iterator<Item = &'b Vec<StateIndex>>>(&'b mut self, iter: I) {
        let features = self.automaton.extract_features(iter.flat_map(|s| s.iter()));
        self.feature_map = zielonka_normal_form_to_labelling(&features);
    }

    fn get_label(&self, indices: &Vec<StateIndex>) -> StructuredLabel {
        let mut sorted_indices = indices.clone();
        sorted_indices.sort();
        self.get_label(&sorted_indices)
    }
}

fn zielonka_normal_form_to_labelling(
    state_features: &HashMap<StateIndex, ZielonkaNormalFormState>,
) -> HashMap<StateIndex, StructuredLabel> {
    // compute local widths
    let mut round_robin_counters_width = 0;
    let mut zielonka_path_width = 0;
    let mut state_map_width = 0;
    let mut state_map_local_widths = HashMap::new();
    for feature in state_features.values() {
        round_robin_counters_width = std::cmp::max(
            round_robin_counters_width,
            feature.round_robin_counters().len(),
        );
        zielonka_path_width = std::cmp::max(zielonka_path_width, feature.zielonka_path().len());
        for (&key, entry) in feature.state_map() {
            state_map_width = std::cmp::max(state_map_width, key as usize + 1);
            let (ref mut all_width, ref mut rejecting_width) =
                state_map_local_widths.entry(key as usize).or_insert((0, 0));
            for val in entry.all_profile() {
                *all_width = std::cmp::max(*all_width, *val as usize + 1);
            }
            for val in entry.rejecting_profile() {
                *rejecting_width = std::cmp::max(*rejecting_width, *val as usize + 1);
            }
        }
    }
    // compute new vectors
    let width = 1 // state formula
        + round_robin_counters_width
        + zielonka_path_width
        + state_map_width // disambiguiation
        + state_map_local_widths
            .values()
            .map(|&(a, r)| a + r)
            .sum::<usize>();

    debug!("State feature space has dimension {}", width);

    let mut map = HashMap::new();
    for (&state, features) in state_features {
        let mut vec: Vec<LabelValue> = Vec::with_capacity(width);
        // add state formula
        vec.push(LabelValue::Value(
            features.state_formula() as LabelInnerValue
        ));
        // add round-robin counter
        vec.extend(
            features
                .round_robin_counters()
                .iter()
                .map(|&v| LabelValue::Value(v as LabelInnerValue)),
        );
        vec.extend(
            iter::repeat(LabelValue::DontCare)
                .take(round_robin_counters_width - features.round_robin_counters().len()),
        );
        // add Zielonka path
        vec.extend(
            features
                .zielonka_path()
                .iter()
                .map(|&v| LabelValue::Value(v as LabelInnerValue)),
        );
        vec.extend(
            iter::repeat(LabelValue::DontCare)
                .take(zielonka_path_width - features.zielonka_path().len()),
        );
        // add state profiles
        for key in 0..state_map_width {
            let (all_width, rejecting_width) =
                state_map_local_widths.get(&key).cloned().unwrap_or((0, 0));
            match features.state_map().get(&(key as i32)) {
                Some(state_entry) => {
                    // disambiguiation
                    vec.push(LabelValue::Value(
                        state_entry.disambiguation() as LabelInnerValue
                    ));
                    // all profile
                    for val in 0..all_width {
                        vec.push(LabelValue::Value(
                            state_entry.all_profile().contains(&(val as i32)) as LabelInnerValue,
                        ));
                    }
                    // rejecting profile
                    for val in 0..rejecting_width {
                        vec.push(LabelValue::Value(
                            state_entry.rejecting_profile().contains(&(val as i32))
                                as LabelInnerValue,
                        ));
                    }
                }
                None => {
                    // disambiguiation + local widths
                    vec.extend(
                        iter::repeat(LabelValue::DontCare).take(1 + all_width + rejecting_width),
                    )
                }
            }
        }
        assert_eq!(vec.len(), width);
        map.insert(state, StructuredLabel::new(vec));
    }
    map
}
