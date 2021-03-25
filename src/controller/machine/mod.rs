mod minimization;

use std::collections::{hash_map::Entry, HashMap, VecDeque};
use std::fmt;
use std::hash::Hash;
use std::ops::Index;

use cudd::{Bdd, CubeValue, Cudd, ReorderingMethod};
use log::info;

use super::bdd::BddController;
use super::labelling::{LabelValue, Labelling, StructuredLabel};

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub(crate) struct StateIndex(usize);

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
struct TransitionOutput {
    output: Bdd,
    successor: StateIndex,
}

impl TransitionOutput {
    fn new(output: Bdd, successor: StateIndex) -> Self {
        Self { output, successor }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Transition {
    input: Bdd,
    outputs: Vec<TransitionOutput>,
}

impl Transition {
    pub(crate) fn new(input: Bdd) -> Self {
        Self::with_outputs(input, Vec::new())
    }

    fn with_outputs(input: Bdd, outputs: Vec<TransitionOutput>) -> Self {
        Self { input, outputs }
    }

    pub(crate) fn add_output(&mut self, output: Bdd, successor: StateIndex) {
        self.outputs.push(TransitionOutput::new(output, successor));
    }
}

#[derive(Debug, Clone)]
pub(crate) struct State<L> {
    label: L,
    transitions: Vec<Transition>,
}

impl<L> State<L> {
    fn new(label: L) -> Self {
        Self::with_transitions(label, Vec::new())
    }

    fn with_transitions(label: L, transitions: Vec<Transition>) -> Self {
        Self { label, transitions }
    }

    fn add_transition(&mut self, transition: Transition) {
        self.transitions.push(transition);
    }

    fn label(&self) -> &L {
        &self.label
    }
}

pub(crate) struct LabelledMachineConstructor<L> {
    states: Vec<State<L>>,
    mapping: HashMap<L, StateIndex>,
}
impl<L: Hash + Eq + Clone> LabelledMachineConstructor<L> {
    pub(crate) fn new() -> Self {
        Self {
            states: Vec::with_capacity(4096),
            mapping: HashMap::with_capacity(4096),
        }
    }

    pub(crate) fn add_state(&mut self, label: L) -> (StateIndex, bool) {
        match self.mapping.entry(label) {
            Entry::Occupied(entry) => (*entry.get(), false),
            Entry::Vacant(entry) => {
                // new state
                let state = State::new(entry.key().clone());
                let index = StateIndex(self.states.len());
                self.states.push(state);
                entry.insert(index);
                (index, true)
            }
        }
    }

    pub(crate) fn add_transition(&mut self, state: StateIndex, transition: Transition) {
        self.states[state.0].add_transition(transition);
    }

    pub(crate) fn into_machine(
        self,
        initial_state: StateIndex,
        inputs: Vec<String>,
        outputs: Vec<String>,
        mealy: bool,
    ) -> LabelledMachine<L> {
        LabelledMachine {
            states: self.states,
            inputs,
            outputs,
            initial_state,
            mealy,
        }
    }
}

#[derive(Debug, Clone)]
pub struct LabelledMachine<L> {
    states: Vec<State<L>>,
    inputs: Vec<String>,
    outputs: Vec<String>,
    initial_state: StateIndex,
    mealy: bool,
}

impl<L> LabelledMachine<L> {
    pub(crate) fn num_states(&self) -> usize {
        self.states.len()
    }

    fn num_inputs(&self) -> usize {
        self.inputs.len()
    }

    fn num_outputs(&self) -> usize {
        self.outputs.len()
    }

    fn num_vars(&self) -> usize {
        self.num_inputs() + self.num_outputs()
    }

    fn states(&self) -> impl Iterator<Item = &State<L>> {
        self.states.iter()
    }

    fn labels(&self) -> impl Iterator<Item = &L> {
        self.states().map(State::label)
    }

    fn is_deterministic(&self) -> bool {
        if self.states.is_empty() {
            return false;
        }
        for state in &self.states {
            if self.mealy {
                for transition in &state.transitions {
                    if transition.outputs.len() != 1 {
                        return false;
                    }
                    if transition.outputs[0]
                        .output
                        .cube_iter(self.num_outputs())
                        .count()
                        != 1
                    {
                        return false;
                    }
                }
            } else {
                if state.transitions.len() != 1 {
                    return false;
                }
                if state.transitions[0]
                    .input
                    .cube_iter(self.num_inputs())
                    .count()
                    != 1
                {
                    return false;
                }
            }
        }
        true
    }

    fn clone_with<LNew>(
        &self,
        new_states: Vec<State<LNew>>,
        new_initial_state: StateIndex,
    ) -> LabelledMachine<LNew> {
        LabelledMachine {
            states: new_states,
            inputs: self.inputs.clone(),
            outputs: self.outputs.clone(),
            initial_state: new_initial_state,
            mealy: self.mealy,
        }
    }

    pub(crate) fn with_structured_labels<F: Labelling<L>>(
        &self,
        labelling: &mut F,
    ) -> LabelledMachine<StructuredLabel> {
        info!("Applying structured labels to automaton");

        labelling.prepare_labels(self.labels());
        let new_states = self
            .states()
            .map(|s| State {
                label: labelling.get_label(s.label()),
                transitions: s.transitions.clone(),
            })
            .collect();
        self.clone_with(new_states, self.initial_state)
    }

    fn state_indices(&self) -> impl Iterator<Item = StateIndex> {
        (0..self.num_states()).map(StateIndex)
    }

    fn states_with_index(&self) -> impl Iterator<Item = (StateIndex, &State<L>)> {
        self.states().enumerate().map(|(i, s)| (StateIndex(i), s))
    }
}

fn keep_max_by_key<T, B: Ord, F>(vec: &mut Vec<T>, mut f: F)
where
    F: FnMut(&T) -> B,
{
    let max_index = vec
        .iter()
        .enumerate()
        .max_by_key(|(_, v)| f(v))
        .map(|(i, _)| i)
        .expect("empty array");
    vec.swap(0, max_index);
    vec.truncate(1);
}

impl<L: Clone> LabelledMachine<L> {
    pub(crate) fn determinize(&mut self) {
        info!("Determinizing machine with {} states", self.num_states());
        let num_inputs = self.num_inputs();
        let num_outputs = self.num_outputs();
        // count how often each input, output and successor state is used
        let mut input_count = HashMap::new();
        let mut output_count = HashMap::new();
        let mut successor_count = HashMap::new();
        for state in &self.states {
            for transition in &state.transitions {
                for input in transition.input.bdd_cube_iter(num_inputs) {
                    *input_count.entry(input).or_insert(0_usize) += 1;
                }
                for output in &transition.outputs {
                    *successor_count.entry(output.successor).or_insert(0_usize) += 1;
                    for output_bdd in output.output.bdd_cube_iter(num_outputs) {
                        *output_count.entry(output_bdd).or_insert(0_usize) += 1;
                    }
                }
            }
        }
        if self.mealy {
            // keep most used successor and then most used output in each transition
            for state in &mut self.states {
                for transition in &mut state.transitions {
                    keep_max_by_key(&mut transition.outputs, |o| successor_count[&o.successor]);
                    let output_bdd = transition.outputs[0]
                        .output
                        .bdd_cube_iter(num_outputs)
                        .max_by_key(|o| output_count[o])
                        .unwrap();
                    transition.outputs[0].output = output_bdd;
                }
            }
        } else {
            // keep inputs with most used successors and most used input
            for state in &mut self.states {
                keep_max_by_key(&mut state.transitions, |t| {
                    t.outputs
                        .iter()
                        .map(|o| successor_count[&o.successor])
                        .sum::<usize>()
                });
                let input_bdd = state.transitions[0]
                    .input
                    .bdd_cube_iter(num_inputs)
                    .max_by_key(|i| input_count[i])
                    .unwrap();
                state.transitions[0].input = input_bdd;
            }
        }

        // remove unreachable states
        let keep = self.reachable_states();
        if keep.iter().any(std::ops::Not::not) {
            *self = self.remove_states(&keep);
        }
        info!("Determinized machine has {} states", self.num_states());
    }

    fn reachable_states(&self) -> Vec<bool> {
        let n = self.num_states();
        let mut reachable = vec![false; n];
        let mut queue = VecDeque::with_capacity(n);
        reachable[self.initial_state.0] = true;
        queue.push_back(self.initial_state);
        while let Some(state_index) = queue.pop_front() {
            let state = &self[state_index];
            for transition in &state.transitions {
                for output in &transition.outputs {
                    let successor = output.successor;
                    if !reachable[successor.0] {
                        reachable[successor.0] = true;
                        queue.push_back(successor);
                    }
                }
            }
        }
        reachable
    }

    fn remove_states(&self, keep: &[bool]) -> Self {
        let n = self.num_states();

        // remap states
        let mut state_mapping = Vec::with_capacity(n);
        let mut new_states: Vec<State<L>> = Vec::with_capacity(n);
        for (index, state) in self.states_with_index() {
            if keep[index.0] {
                let new_index = new_states.len();
                new_states.push(State::new(state.label().clone()));
                state_mapping.push(new_index);
            } else {
                state_mapping.push(0);
            }
        }

        // update transitions
        for (index, state) in self.states_with_index() {
            if keep[index.0] {
                let new_index = state_mapping[index.0];
                let new_state = &mut new_states[new_index];
                for transition in &state.transitions {
                    let mut new_transition = Transition::new(transition.input.clone());
                    for output in &transition.outputs {
                        let successor_index = output.successor.0;
                        if keep[successor_index] {
                            let new_successor = StateIndex(state_mapping[successor_index]);
                            new_transition.add_output(output.output.clone(), new_successor);
                        }
                    }
                    if self.mealy {
                        assert!(!new_transition.outputs.is_empty());
                        new_state.add_transition(new_transition);
                    } else if new_transition.outputs.len() == transition.outputs.len() {
                        new_state.add_transition(new_transition);
                    }
                }
                assert!(!new_state.transitions.is_empty());
            }
        }
        // create new machine
        assert!(keep[self.initial_state.0]);
        let new_initial_state = StateIndex(state_mapping[self.initial_state.0]);
        self.clone_with(new_states, new_initial_state)
    }

    pub(crate) fn minimize_with_nondeterminism(&self) -> Self {
        info!(
            "Minimizing machine with {} states using successor non-determinism",
            self.num_states()
        );

        let reachable_states = self.minimal_reachable_states();
        let new_machine = self.remove_states(&reachable_states);
        info!("Minimized machine has {} states", new_machine.num_states());
        new_machine
    }

    pub(crate) fn minimize_with_dontcares(&self) -> LabelledMachine<Vec<L>> {
        info!(
            "Minimizing machine with {} states using don't cares",
            self.num_states()
        );
        assert!(
            self.is_deterministic(),
            "can only minimize using don't cares from deterministic machine"
        );

        let n = self.num_states();
        let matrix = self.compute_incompatability_matrix();
        let classes = matrix.compute_transitively_compatible_states();
        let pairwise_incompatible_states = self.find_pairwise_incompatible_states(&matrix);
        let lower_bound = pairwise_incompatible_states.len();
        assert!((1..=n).contains(&lower_bound));

        if lower_bound < n {
            let split_machine = self.split_actions(&classes);
            for num_states in lower_bound..n {
                if let Some(min_machine) = split_machine.find_covering_machine(
                    num_states,
                    &matrix,
                    &pairwise_incompatible_states,
                ) {
                    info!(
                        "Minimized machine to {} states using don't cares",
                        min_machine.num_states()
                    );
                    return min_machine;
                }
            }
        }
        // no further minimization possible, return copy of current machine
        let new_states = self
            .states()
            .map(|state| {
                State::with_transitions(vec![state.label().clone()], state.transitions.clone())
            })
            .collect();
        info!("No further minimization using don't cares possible");
        self.clone_with(new_states, self.initial_state)
    }
}

fn bdd_for_label(
    label: &StructuredLabel,
    manager: &Cudd,
    var_offset: usize,
    widths: &[u32],
) -> Bdd {
    let mut bdd = manager.bdd_one();
    let mut var = 0;
    for (v, &w) in label.iter().zip(widths.iter()) {
        for i in 0..w {
            let bdd_var = manager.bdd_var(var_offset + var);
            if let LabelValue::Value(val) = v {
                if val & (1 << i) == 0 {
                    bdd &= !bdd_var;
                } else {
                    bdd &= bdd_var;
                }
            }
            var += 1;
        }
    }
    bdd
}

fn bits_for_label(label: &StructuredLabel, widths: &[u32]) -> Vec<bool> {
    label
        .iter()
        .zip(widths.iter())
        .flat_map(|(&v, &w)| (0..w).map(move |i| v.bit(i)))
        .collect()
}

impl LabelledMachine<StructuredLabel> {
    pub(crate) fn create_bdds(&self) -> BddController {
        info!("Constructing BDD from machine");
        assert!(
            self.is_deterministic(),
            "can only create BDDs from deterministic machine"
        );
        // TODO compress labels here

        // compute bit widths of each label
        let initial_label = self[self.initial_state].label();
        let components = initial_label.components();
        let mut widths = vec![0; components];
        for state in &self.states {
            let label = state.label();
            assert_eq!(label.components(), components);
            for (w, &v) in widths.iter_mut().zip(label.iter()) {
                *w = std::cmp::max(*w, v.num_bits());
            }
        }
        let num_state_vars = widths.iter().sum::<u32>() as usize;
        let num_controllable_vars = if self.mealy {
            self.num_outputs()
        } else {
            self.num_inputs()
        };
        let num_uncontrollable_vars = self.num_vars() - num_controllable_vars;
        let num_vars = num_uncontrollable_vars + num_state_vars;

        let mut manager = Cudd::with_vars(num_vars).unwrap();
        manager.autodyn_enable(ReorderingMethod::Sift);

        let mut successor_bdds = vec![manager.bdd_zero(); num_state_vars];
        let mut controlled_bdds = vec![manager.bdd_zero(); num_controllable_vars];

        for state in &self.states {
            let state_bdd =
                bdd_for_label(state.label(), &manager, num_uncontrollable_vars, &widths);
            if self.mealy {
                for transition in &state.transitions {
                    let input_bdd = transition.input.transfer(&manager);
                    let combined_bdd = input_bdd & &state_bdd;
                    // get first cube and successor of first output
                    let transition_output = &transition.outputs[0];
                    let cube_out = transition_output
                        .output
                        .cube_iter(self.num_outputs())
                        .next()
                        .unwrap();
                    let successor_label = self[transition_output.successor].label();
                    let successor_bits = bits_for_label(successor_label, &widths);
                    for (bdd, v) in controlled_bdds.iter_mut().zip(cube_out.iter()) {
                        if *v == CubeValue::Set {
                            *bdd |= &combined_bdd;
                        }
                    }
                    for (var, bdd) in successor_bdds.iter_mut().enumerate() {
                        if successor_bits[var] {
                            *bdd |= &combined_bdd;
                        }
                    }
                }
            } else {
                // get first cube of first input
                let transition = &state.transitions[0];
                let cube_in = transition
                    .input
                    .cube_iter(self.num_inputs())
                    .next()
                    .unwrap();
                for (bdd, v) in controlled_bdds.iter_mut().zip(cube_in.iter()) {
                    if *v == CubeValue::Set {
                        *bdd |= &state_bdd;
                    }
                }
                for transition_output in &transition.outputs {
                    let output_bdd = transition_output.output.transfer(&manager);
                    let combined_bdd = output_bdd & &state_bdd;
                    let successor_label = self[transition_output.successor].label();
                    let successor_bits = bits_for_label(successor_label, &widths);
                    for (var, bdd) in successor_bdds.iter_mut().enumerate() {
                        if successor_bits[var] {
                            *bdd |= &combined_bdd;
                        }
                    }
                }
            }
        }
        manager.autodyn_disable();

        let initial_bits = bits_for_label(initial_label, &widths);
        let (bdd_inputs, bdd_outputs) = if self.mealy {
            (&self.inputs, &self.outputs)
        } else {
            (&self.outputs, &self.inputs)
        };
        BddController::new(
            bdd_inputs.clone(),
            bdd_outputs.clone(),
            initial_bits,
            successor_bdds,
            controlled_bdds,
            manager,
        )
    }
}

impl<L> Index<StateIndex> for LabelledMachine<L> {
    type Output = State<L>;

    fn index(&self, index: StateIndex) -> &Self::Output {
        &self.states[index.0]
    }
}

impl fmt::Display for StateIndex {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl fmt::Display for Transition {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for out in &self.outputs {
            writeln!(f, "[({}) & ({})] {}", self.input, out.output, out.successor)?;
        }
        Ok(())
    }
}

impl<L: fmt::Display> fmt::Display for State<L> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln!(f, "\"{}\"", self.label())?;
        for t in &self.transitions {
            write!(f, "{}", t)?;
        }
        Ok(())
    }
}

impl<L: fmt::Display> fmt::Display for LabelledMachine<L> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let input_names: Vec<_> = (0..self.num_inputs()).map(|i| format!("{}", i)).collect();
        let output_names: Vec<_> = (self.num_inputs()..self.num_vars())
            .map(|i| format!("{}", i))
            .collect();

        // write header
        writeln!(f, "HOA: v1")?;
        writeln!(
            f,
            "tool: \"{}\" \"{}\"",
            env!("CARGO_PKG_NAME"),
            env!("CARGO_PKG_VERSION")
        )?;
        writeln!(f, "States: {}", self.num_states())?;
        writeln!(f, "Start: {}", self.initial_state)?;
        write!(f, "AP: {}", self.num_vars())?;
        for input in &self.inputs {
            write!(f, " \"{}\"", input)?;
        }
        for output in &self.outputs {
            write!(f, " \"{}\"", output)?;
        }
        writeln!(f)?;
        write!(f, "controllable-AP:")?;
        if self.mealy {
            for o in self.num_inputs()..self.num_vars() {
                write!(f, " {}", o)?;
            }
        } else {
            for i in 0..self.num_inputs() {
                write!(f, " {}", i)?;
            }
        }
        writeln!(f)?;
        writeln!(f, "acc-name: all")?;
        writeln!(f, "Acceptance: 0 t")?;

        // write body
        writeln!(f, "--BODY--")?;
        for (index, state) in self.states_with_index() {
            writeln!(f, "State: {} \"{}\"", index, state.label())?;
            for t in &state.transitions {
                let input = t.input.factored_form_string(&input_names);
                for out in &t.outputs {
                    let output = out.output.factored_form_string(&output_names);
                    writeln!(f, "[({}) & ({})] {}", input, output, out.successor)?;
                }
            }
        }
        writeln!(f, "--END--")?;
        Ok(())
    }
}
