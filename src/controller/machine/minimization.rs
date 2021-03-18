use std::collections::{HashMap, HashSet, VecDeque};
use std::ops::Index;

use cudd::BDD;
use log::{debug, error};
use varisat::{ExtendFormula, Lit, Solver};

use super::{LabelledMachine, State, StateIndex, Transition, TransitionOutput};

/// Obtain a model for the constraints already in solver where the minimal
/// number of given vars are set to true.
///
/// Assumes that the model is satisfiable with all vars set to true.
fn minimal_model(solver: &mut Solver, vars: &[Lit]) -> Vec<Lit> {
    /*
    Use sequential-counter based encoding for !vars[0] + .. + !vars[n-1] >= k as in:
        Ben-Haim et al.: Perfect Hashing and CNF Encodings of Cardinality Constraints
    Adapted to work incrementally in a dynamic-programming like fashion.
    */

    let n = vars.len();
    let mut model = vars.to_vec();
    let mut best = 0;

    let mut last_counter: Option<Vec<Lit>> = None;
    for k in 0..n {
        let new_counter: Vec<_> = (k..n).map(|_| solver.new_lit()).collect();

        // initial clause
        solver.add_clause(&[!new_counter[0], !vars[k]]);
        for i in 1..(n - k) {
            // addition clause
            solver.add_clause(&[!new_counter[i], new_counter[i - 1], !vars[i + k]]);
        }
        if let Some(old_counter) = last_counter {
            for i in 0..(n - k) {
                // incremental clause
                solver.add_clause(&[!new_counter[i], old_counter[i]]);
            }
        }
        // blocking clause
        solver.add_clause(&[new_counter[n - k - 1]]);

        last_counter = Some(new_counter);

        // skip over solver steps if a better solution has already been found
        if k < best {
            continue;
        }

        match solver.solve() {
            Ok(false) => break,
            Ok(true) => {
                let sat_model = solver.model().unwrap();
                for var in &mut model {
                    *var = sat_model[var.index()];
                }
                let new_best = model.iter().filter(|var| var.is_negative()).count();
                assert!(new_best > best);
                best = new_best;
            }
            Err(err) => {
                error!("Sat solver error: {}", err);
                break;
            }
        }
    }

    model
}

impl<L: std::fmt::Display> LabelledMachine<L> {
    pub(super) fn minimal_reachable_states(&self) -> Vec<bool> {
        let mut solver = Solver::new();
        let state_vars: Vec<_> = self.state_indices().map(|_| solver.new_lit()).collect();
        // initial state is reachable
        solver.add_clause(&[state_vars[self.initial_state.0]]);
        for (index, state) in self.states_with_index() {
            let state_var = state_vars[index.0];
            if self.mealy {
                // if state is reachable, then for every input some successor is reachable
                for transition in &state.transitions {
                    let mut successor_clause = Vec::with_capacity(1 + transition.outputs.len());
                    successor_clause.push(!state_var);
                    successor_clause
                        .extend(transition.outputs.iter().map(|o| state_vars[o.successor.0]));
                    solver.add_clause(&successor_clause);
                }
            } else {
                // if state is reachable, then for some input every successor is reachable
                let mut input_clause = Vec::with_capacity(1 + state.transitions.len());
                input_clause.push(!state_var);
                input_clause.extend((0..state.transitions.len()).map(|_| solver.new_lit()));
                solver.add_clause(&input_clause);
                for (input_index, transition) in state.transitions.iter().enumerate() {
                    let input_var = input_clause[input_index + 1];
                    for output in &transition.outputs {
                        let successor_var = state_vars[output.successor.0];
                        solver.add_clause(&[!input_var, successor_var]);
                    }
                }
            }
        }

        let minimal_model = minimal_model(&mut solver, &state_vars);
        minimal_model
            .into_iter()
            .map(|lit| lit.is_positive())
            .collect()
    }

    pub(super) fn compute_incompatability_matrix(&self) -> IncompatabilityMatrix {
        IncompatabilityMatrix::new(self)
    }

    /// Returns a list of states such that each state is pairwise incompatible
    /// with all preceeding states.
    pub(super) fn find_pairwise_incompatible_states(
        &self,
        matrix: &IncompatabilityMatrix,
    ) -> Vec<StateIndex> {
        let mut state_num_incomp: Vec<_> = self
            .state_indices()
            .map(|i| {
                (
                    i,
                    self.state_indices()
                        .map(|j| matrix[(i, j)] as usize)
                        .sum::<usize>(),
                )
            })
            .collect();
        state_num_incomp.sort_by_key(|(_, c)| std::cmp::Reverse(*c));

        let mut pairwise_inc_states = Vec::new();
        for (i, _) in state_num_incomp {
            if pairwise_inc_states.iter().all(|&j| matrix[(i, j)]) {
                pairwise_inc_states.push(i);
            }
        }
        pairwise_inc_states
    }

    /// Computes a list of inputs such that all inputs in the list are pairwise disjoint
    /// and their union is equal to the union of the inputs in the given class.
    fn disjoint_input_set(&self, class: &[StateIndex]) -> Vec<BDD> {
        let mut disjoint_set: HashSet<BDD> = HashSet::new();
        let mut queue = VecDeque::new();
        for &i in class {
            for transition in &self[i].transitions {
                if self.mealy {
                    queue.push_back(transition.input.clone());
                } else {
                    for output in &transition.outputs {
                        queue.push_back(output.output.clone());
                    }
                }
            }
        }
        while let Some(input) = queue.pop_front() {
            if disjoint_set.contains(&input) {
                continue;
            }
            let intersection_match = disjoint_set.iter().find_map(|disjoint_input| {
                let intersection = disjoint_input & &input;
                (!intersection.is_zero()).then(|| (intersection, disjoint_input.clone()))
            });
            match intersection_match {
                Some((intersection, disjoint_input)) => {
                    let diff0 = &input & !&intersection;
                    let diff1 = &disjoint_input & !&intersection;
                    if diff0.is_zero() {
                        disjoint_set.remove(&disjoint_input);
                        disjoint_set.insert(intersection);
                        disjoint_set.insert(diff1);
                    } else if diff1.is_zero() {
                        queue.push_back(diff0);
                    } else {
                        disjoint_set.remove(&disjoint_input);
                        queue.push_back(diff0);
                        disjoint_set.insert(intersection);
                        disjoint_set.insert(diff1);
                    }
                }
                None => {
                    disjoint_set.insert(input.clone());
                }
            };
        }
        // TODO remove test assertions
        if let Some(x) = disjoint_set.iter().next() {
            let mut set_union = x.manager().bdd_zero();
            let mut input_union = x.manager().bdd_zero();
            for y in disjoint_set.iter() {
                set_union |= y;
            }
            for &i in class {
                for transition in &self[i].transitions {
                    if self.mealy {
                        input_union |= &transition.input;
                    } else {
                        for output in &transition.outputs {
                            input_union |= &output.output;
                        }
                    }
                }
            }
            assert_eq!(input_union, set_union);
        }
        for i1 in &disjoint_set {
            for i2 in &disjoint_set {
                if i1 != i2 {
                    assert!((i1 & i2).is_zero());
                }
            }
        }
        disjoint_set.into_iter().collect()
    }
}

impl<L: Clone + std::fmt::Display> LabelledMachine<L> {
    /// Returns a copy of the current machine where all inputs in transitions
    /// of states in the same equivalence class are pairwise disjoint.
    ///
    /// Additionally ensures that the list of inputs for all states in the same class
    /// is the same.
    pub(super) fn split_inputs(&self, classes: &StateEquivalenceClasses) -> LabelledMachine<L> {
        debug!("Splitting input sets");
        let mut new_states: Vec<State<L>> = self
            .states()
            .map(|state| State::new(state.label().clone()))
            .collect();
        for class in &classes.classes {
            let disjoint_set = self.disjoint_input_set(class);
            for &i in class {
                let state = &self[i];
                let new_state = &mut new_states[i.0];
                if self.mealy {
                    for transition in &state.transitions {
                        let input = &transition.input;
                        new_state.transitions.extend(
                            disjoint_set
                                .iter()
                                .filter(|&new_input| !(new_input & input).is_zero())
                                .map(|new_input| {
                                    Transition::with_outputs(
                                        new_input.clone(),
                                        transition.outputs.clone(),
                                    )
                                }),
                        );
                    }
                    new_state.transitions.sort_by_key(|t| t.input.node_id());
                } else {
                    let transition = &state.transitions[0];
                    let mut new_transition = Transition::new(transition.input.clone());
                    for transition_output in &transition.outputs {
                        let output = &transition_output.output;
                        let successor = transition_output.successor;
                        new_transition.outputs.extend(
                            disjoint_set
                                .iter()
                                .filter(|&new_output| !(new_output & output).is_zero())
                                .map(|new_output| {
                                    TransitionOutput::new(new_output.clone(), successor)
                                }),
                        );
                    }
                    new_transition.outputs.sort_by_key(|to| to.output.node_id());
                    new_state.add_transition(new_transition)
                }
            }
        }
        debug!("Done splitting input sets");
        self.clone_with(new_states, self.initial_state)
    }

    /// Find a machine with num_states states that covers the current machine.
    ///
    /// Uses Approach described in Abel and Reineke:
    /// MEMIN: SAT-based Exact Minimization ofIncompletely Specified Mealy Machines
    pub(super) fn find_covering_machine(
        &self,
        num_states: usize,
        matrix: &IncompatabilityMatrix,
        pairwise_incompatible_states: &[StateIndex],
    ) -> Option<LabelledMachine<Vec<L>>> {
        if !self.mealy {
            // TODO also adapt for Moore machines
            return None;
        }

        let mut solver = Solver::new();

        // class_state_vars[i][s] should be true if class i contains state s
        let class_state_vars: Vec<Vec<_>> = (0..num_states)
            .map(|_| self.state_indices().map(|_| solver.new_lit()).collect())
            .collect();

        // every state is in some class
        for s in self.state_indices() {
            let class_vars: Vec<_> = (0..num_states).map(|i| class_state_vars[i][s.0]).collect();
            solver.add_clause(&class_vars);
        }

        // assign pairwise incompatible states to different classes
        for (i, s) in pairwise_incompatible_states.iter().enumerate() {
            solver.add_clause(&[class_state_vars[i][s.0]]);
        }

        // compute list of states that could be in each class
        let possible_states_in_class: Vec<Vec<_>> = (0..num_states)
            .map(|i| {
                self.state_indices()
                    .filter(|&s1| {
                        pairwise_incompatible_states
                            .get(i)
                            .map_or(true, |&s2| !matrix[(s1, s2)])
                    })
                    .collect()
            })
            .collect();

        // incompatible states must not be in the same class
        for (i, state_vars) in class_state_vars.iter().enumerate() {
            for s1 in self.state_indices() {
                match pairwise_incompatible_states.get(i) {
                    Some(&s2) if matrix[(s1, s2)] => solver.add_clause(&[!state_vars[s1.0]]),
                    _ => {
                        for s2 in ((s1.0 + 1)..self.num_states())
                            .map(StateIndex)
                            .filter(|&s2| matrix[(s1, s2)])
                        {
                            solver.add_clause(&[!state_vars[s1.0], !state_vars[s2.0]]);
                        }
                    }
                }
            }
        }

        // compute maximum index for inputs
        // assumes that splitTransitions has been called before
        let num_inputs = self.states().map(|s| s.transitions.len()).max().unwrap();

        // mapping for successor variables
        // the tuple (j, var) in successor_vars[i][a] has var set to true if
        // j is the successor in class i under input a
        let mut class_successors: Vec<Vec<Vec<(usize, Lit)>>> = Vec::with_capacity(num_states);

        // closure constraints
        for (i, possible_states) in possible_states_in_class.iter().enumerate() {
            let mut class_successor_mapping = Vec::with_capacity(num_inputs);
            for a in 0..num_inputs {
                // compute possible successor classes
                let mut successor_classes = HashSet::with_capacity(num_states);
                for &s in possible_states {
                    if let Some(transition) = self[s].transitions.get(a) {
                        let successor = transition.outputs[0].successor;

                        successor_classes.extend((0..num_states).filter(|&j| {
                            pairwise_incompatible_states
                                .get(j)
                                .map_or(true, |&s2| !matrix[(successor, s2)])
                        }));
                    }
                }
                let successor_mapping;
                if successor_classes.is_empty() {
                    successor_mapping = Vec::new();
                } else {
                    successor_mapping = successor_classes
                        .into_iter()
                        .map(|j| (j, solver.new_lit()))
                        .collect();

                    // clause for disjunction over successor
                    let successor_vars: Vec<_> =
                        successor_mapping.iter().map(|(_, var)| *var).collect();
                    solver.add_clause(&successor_vars);

                    for &s in possible_states {
                        if let Some(transition) = self[s].transitions.get(a) {
                            let successor = transition.outputs[0].successor;
                            for &(j, var) in &successor_mapping {
                                solver.add_clause(&[
                                    !var,
                                    !class_state_vars[i][s.0],
                                    class_state_vars[j][successor.0],
                                ])
                            }
                        }
                    }
                }
                class_successor_mapping.push(successor_mapping)
            }
            class_successors.push(class_successor_mapping);
        }

        match solver.solve() {
            Ok(true) => {
                // obtain class covering and successors
                let model = solver.model().unwrap();
                let classes: Vec<Vec<_>> = class_state_vars
                    .into_iter()
                    .map(|state_vars| {
                        state_vars
                            .into_iter()
                            .enumerate()
                            .filter(|(_, var)| model[var.index()].is_positive())
                            .map(|(j, _)| StateIndex(j))
                            .collect()
                    })
                    .collect();
                let successors: Vec<Vec<Vec<_>>> = class_successors
                    .into_iter()
                    .map(|input_mapping| {
                        input_mapping
                            .into_iter()
                            .map(|successor_mapping| {
                                successor_mapping
                                    .into_iter()
                                    .filter(|(_, var)| model[var.index()].is_positive())
                                    .map(|(j, _)| StateIndex(j))
                                    .collect()
                            })
                            .collect()
                    })
                    .collect();
                Some(self.build_machine_from_classes(classes, successors))
            }
            Ok(false) => None,
            Err(err) => {
                error!("Sat solver error: {}", err);
                None
            }
        }
    }

    /// Builds a machine from the given set of compatability classes
    /// and their respective successors.
    fn build_machine_from_classes(
        &self,
        classes: Vec<Vec<StateIndex>>,
        class_successors: Vec<Vec<Vec<StateIndex>>>,
    ) -> LabelledMachine<Vec<L>> {
        let initial_state = classes
            .iter()
            .enumerate()
            .find(|(_, class)| class.contains(&self.initial_state))
            .map(|(i, _)| StateIndex(i))
            .unwrap();

        let new_states = classes
            .into_iter()
            .zip(class_successors.into_iter())
            .map(|(class, successors)| {
                assert!(!class.is_empty());
                let class_states: Vec<_> = class.into_iter().map(|s| &self[s]).collect();
                let new_label = class_states.iter().map(|s| s.label().clone()).collect();

                let rep_state = class_states[0];
                let num_inputs = rep_state.transitions.len();
                assert!(class_states
                    .iter()
                    .all(|s| s.transitions.len() == num_inputs));
                assert!(successors.len() >= num_inputs);

                let new_transitions = successors
                    .into_iter()
                    .enumerate()
                    .take(num_inputs)
                    .map(|(a, input_successors)| {
                        assert!(!input_successors.is_empty());
                        let input = rep_state.transitions[a].input.clone();
                        assert!(class_states.iter().all(|s| s.transitions[a].input == input));
                        let successor = input_successors[0];
                        let initial_output = rep_state.transitions[a].outputs[0].output.clone();
                        let output = class_states
                            .iter()
                            .skip(1)
                            .map(|&s| &s.transitions[a].outputs[0].output)
                            .fold(initial_output, |o1, o2| o1 & o2);
                        assert!(!output.is_zero());
                        Transition::with_outputs(
                            input,
                            vec![TransitionOutput::new(output, successor)],
                        )
                    })
                    .collect();

                State::with_transitions(new_label, new_transitions)
            })
            .collect();

        self.clone_with(new_states, initial_state)
    }
}

struct PredecessorMapEntry {
    input: BDD,
    predecessors: Vec<StateIndex>,
}

struct PredecessorMap {
    map: Vec<Vec<PredecessorMapEntry>>,
}

impl PredecessorMap {
    fn new<L>(machine: &LabelledMachine<L>) -> Self {
        let mut map = vec![HashMap::new(); machine.num_states()];
        for (i, state) in machine.states_with_index() {
            if machine.mealy {
                for transition in &state.transitions {
                    assert!(transition.outputs.len() == 1);
                    let input = transition.input.clone();
                    let successor = transition.outputs[0].successor.0;
                    map[successor].entry(input).or_insert_with(Vec::new).push(i);
                }
            } else {
                assert!(state.transitions.len() == 1);
                for output in &state.transitions[0].outputs {
                    let successor = output.successor.0;
                    let output = output.output.clone();
                    map[successor]
                        .entry(output)
                        .or_insert_with(Vec::new)
                        .push(i);
                }
            }
        }
        Self::from(map)
    }

    fn from(hash_maps: Vec<HashMap<BDD, Vec<StateIndex>>>) -> Self {
        let map = hash_maps
            .into_iter()
            .map(|m| {
                m.into_iter()
                    .map(|(input, predecessors)| PredecessorMapEntry {
                        input,
                        predecessors,
                    })
                    .collect()
            })
            .collect();
        PredecessorMap { map }
    }
}

impl Index<StateIndex> for PredecessorMap {
    type Output = [PredecessorMapEntry];

    fn index(&self, index: StateIndex) -> &Self::Output {
        &self.map[index.0]
    }
}

pub(super) struct IncompatabilityMatrix {
    n: usize,
    incompatible: Vec<bool>,
}

impl IncompatabilityMatrix {
    fn new<L>(machine: &LabelledMachine<L>) -> Self {
        debug!("Computing predecessor map");
        let map = PredecessorMap::new(machine);
        debug!("Computing incompatability matrix");
        let n = machine.num_states();
        let mut matrix = IncompatabilityMatrix {
            n,
            incompatible: vec![false; n * n],
        };
        for (i, s1) in machine.states_with_index() {
            for (j, s2) in machine.states_with_index().skip(i.0 + 1) {
                if !matrix[(i, j)] && Self::incompatible(machine.mealy, s1, s2) {
                    matrix.set(i, j);
                    matrix.propagate(i, j, &map);
                }
            }
        }
        matrix
    }

    fn incompatible<L>(mealy: bool, s1: &State<L>, s2: &State<L>) -> bool {
        if mealy {
            for t1 in &s1.transitions {
                for t2 in &s2.transitions {
                    if !(&t1.input & &t2.input).is_zero()
                        && (&t1.outputs[0].output & &t2.outputs[0].output).is_zero()
                    {
                        return true;
                    }
                }
            }
            false
        } else {
            (&s1.transitions[0].input & &s2.transitions[0].input).is_zero()
        }
    }

    fn propagate(&mut self, i: StateIndex, j: StateIndex, map: &PredecessorMap) {
        let mut queue = VecDeque::with_capacity(self.n);
        queue.push_back((i, j));
        while let Some((i, j)) = queue.pop_front() {
            for pre1 in &map[i] {
                for pre2 in &map[j] {
                    if !(&pre1.input & &pre2.input).is_zero() {
                        for &s1 in &pre1.predecessors {
                            for &s2 in &pre2.predecessors {
                                if !self[(s1, s2)] {
                                    self.set(s1, s2);
                                    queue.push_back((s1, s2));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    fn set(&mut self, i: StateIndex, j: StateIndex) {
        self.incompatible[i.0 * self.n + j.0] = true;
        self.incompatible[j.0 * self.n + i.0] = true;
    }

    fn state_indices(&self) -> impl Iterator<Item = StateIndex> {
        (0..self.n).map(StateIndex)
    }

    pub(super) fn compute_transitively_compatible_states(&self) -> StateEquivalenceClasses {
        let mut classes = Vec::with_capacity(self.n);

        let mut processed = vec![false; self.n];
        for i in self.state_indices() {
            if !processed[i.0] {
                processed[i.0] = true;
                let mut current_class = Vec::with_capacity(self.n);
                current_class.push(i);

                let mut queue = VecDeque::with_capacity(self.n);
                queue.push_back(i);
                while let Some(i) = queue.pop_front() {
                    for j in self.state_indices() {
                        if !processed[j.0] && !self[(i, j)] {
                            processed[j.0] = true;
                            current_class.push(j);
                            queue.push_back(j);
                        }
                    }
                }
                classes.push(current_class);
            }
        }
        StateEquivalenceClasses { classes }
    }
}

impl Index<(StateIndex, StateIndex)> for IncompatabilityMatrix {
    type Output = bool;

    fn index(&self, index: (StateIndex, StateIndex)) -> &Self::Output {
        let (i, j) = index;
        &self.incompatible[i.0 * self.n + j.0]
    }
}

pub(super) struct StateEquivalenceClasses {
    classes: Vec<Vec<StateIndex>>,
}
