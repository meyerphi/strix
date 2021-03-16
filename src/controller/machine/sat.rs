use std::collections::{HashMap, VecDeque};

use cudd::BDD;
use log::{debug, error};
use varisat::{ExtendFormula, Lit, Solver};

use super::{LabelledMachine, State, StateIndex};

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

impl<L> LabelledMachine<L> {
    pub(super) fn minimal_reachable_states(&self) -> Vec<bool> {
        let n = self.num_states();
        let mut solver = Solver::new();
        let state_vars: Vec<_> = (0..n).map(|_| solver.new_lit()).collect();
        // initial state is reachable
        solver.add_clause(&[state_vars[self.initial_state.0]]);
        for (index, state) in self.states.iter().enumerate() {
            let state_var = state_vars[index];
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

    fn incompatible(&self, s1: &State<L>, s2: &State<L>) -> bool {
        if self.mealy {
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

    fn compute_predecessor_map(&self) -> PredecessorMap {
        debug!("Computing predecessor map");
        let mut map = vec![HashMap::new(); self.num_states()];
        for (i, state) in self.states.iter().enumerate() {
            if self.mealy {
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
        PredecessorMap::from(map)
    }

    pub(super) fn compute_incompatability_matrix(&self) -> IncompatabilityMatrix {
        let n = self.num_states();
        let map = self.compute_predecessor_map();
        let mut matrix = IncompatabilityMatrix::new(n);
        debug!("Computing incompatability matrix");
        for (i, s1) in self.states.iter().enumerate() {
            for (j, s2) in self.states.iter().skip(i + 1).enumerate() {
                if !matrix.get(i, j) && self.incompatible(s1, s2) {
                    matrix.set(i, j);
                    matrix.propagate(i, j, &map);
                }
            }
        }
        matrix
    }

    pub(super) fn find_pairwise_incompatible_states(
        &self,
        classes: &StateEquivalenceClasses,
    ) -> Vec<(StateIndex, StateIndex)> {
        vec![(StateIndex(0), StateIndex(0)); classes.len()]
    }
}

struct PredecessorMapEntry {
    input: BDD,
    predecessors: Vec<usize>,
}

struct PredecessorMap {
    map: Vec<Vec<PredecessorMapEntry>>,
}

impl PredecessorMap {
    fn from(hash_maps: Vec<HashMap<BDD, Vec<usize>>>) -> Self {
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

    fn get(&self, state_index: usize) -> &[PredecessorMapEntry] {
        &self.map[state_index]
    }
}

pub(super) struct IncompatabilityMatrix {
    n: usize,
    incompatible: Vec<bool>,
}

impl IncompatabilityMatrix {
    fn new(n: usize) -> Self {
        IncompatabilityMatrix {
            n,
            incompatible: vec![false; n * n],
        }
    }

    fn propagate(&mut self, i: usize, j: usize, map: &PredecessorMap) {
        let mut queue = VecDeque::with_capacity(self.n);
        queue.push_back((i, j));
        while let Some((i, j)) = queue.pop_front() {
            for pre1 in map.get(i) {
                for pre2 in map.get(j) {
                    if !(&pre1.input & &pre2.input).is_zero() {
                        for &s1 in &pre1.predecessors {
                            for &s2 in &pre2.predecessors {
                                if !self.get(s1, s2) {
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

    fn set(&mut self, i: usize, j: usize) {
        self.incompatible[i * self.n + j] = true;
        self.incompatible[j * self.n + i] = true;
    }

    fn get(&self, i: usize, j: usize) -> bool {
        self.incompatible[i * self.n + j]
    }

    pub(super) fn compute_transitively_compatible_states(&self) -> StateEquivalenceClasses {
        let mut classes = Vec::with_capacity(self.n);

        let mut processed = vec![false; self.n];
        for i in 0..self.n {
            if !processed[i] {
                processed[i] = true;
                let mut current_class = Vec::with_capacity(self.n);
                current_class.push(i);

                let mut queue = VecDeque::with_capacity(self.n);
                queue.push_back(i);
                while let Some(i) = queue.pop_front() {
                    #[allow(clippy::needless_range_loop)]
                    for j in 0..self.n {
                        if !processed[j] && !self.get(i, j) {
                            processed[j] = true;
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

pub(super) struct StateEquivalenceClasses {
    classes: Vec<Vec<usize>>,
}

impl StateEquivalenceClasses {
    fn len(&self) -> usize {
        self.classes.len()
    }
}
