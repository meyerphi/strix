pub(crate) mod queue;

use std::collections::VecDeque;
use std::fmt;
use std::time::{Duration, Instant};

use cudd::{Cudd, BDD};
use owl::automaton::{Color, MaxEvenDPA, StateIndex};
use owl::formula::AtomicPropositionStatus;
use owl::tree::{Node as TreeNode, TreeIndex};

use crate::controller::labelling::AutomatonTreeLabel;
use crate::controller::machine::{LabelledMachine, LabelledMachineConstructor, Transition};
use crate::parity::game::{Game, LabelledGame, Node, NodeIndex, Player};
use crate::parity::solver::Strategy;
use queue::ExplorationQueue;

#[derive(Debug, Default, Clone)]
pub(crate) struct ExplorationStats {
    states: usize,
    edges: usize,
    nodes: usize,
    time: Duration,
}

impl ExplorationStats {
    fn new(states: usize, edges: usize, nodes: usize, time: Duration) -> Self {
        Self {
            states,
            edges,
            nodes,
            time,
        }
    }

    pub(crate) fn states(&self) -> usize {
        self.states
    }

    pub(crate) fn edges(&self) -> usize {
        self.edges
    }

    pub(crate) fn nodes(&self) -> usize {
        self.nodes
    }

    pub(crate) fn time(&self) -> Duration {
        self.time
    }
}

impl std::ops::AddAssign for ExplorationStats {
    fn add_assign(&mut self, rhs: Self) {
        self.states += rhs.states;
        self.edges += rhs.edges;
        self.nodes += rhs.nodes;
        self.time += rhs.time;
    }
}

impl fmt::Display for ExplorationStats {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "|Q| = {}, |E| = {}, |V| = {}, exploration time: {:.2}",
            self.states(),
            self.edges(),
            self.nodes(),
            self.time().as_secs_f32(),
        )
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum ExplorationLimit {
    None,
    Nodes(usize),
    Edges(usize),
    States(usize),
    Time(Duration),
}
pub(crate) struct AutomatonSpecification<A> {
    automaton: A,
    inputs: Vec<String>,
    outputs: Vec<String>,
    statuses: Vec<AtomicPropositionStatus>,
}

impl<A: MaxEvenDPA> AutomatonSpecification<A>
where
    A::EdgeLabel: Clone + Eq + Ord,
{
    pub(crate) fn new<S: AsRef<str>>(
        automaton: A,
        inputs: &[S],
        outputs: &[S],
        statuses: Vec<AtomicPropositionStatus>,
    ) -> Self {
        Self {
            automaton,
            inputs: inputs.iter().map(|s| s.as_ref().to_owned()).collect(),
            outputs: outputs.iter().map(|s| s.as_ref().to_owned()).collect(),
            statuses,
        }
    }
}

pub(crate) struct GameConstructor<A, Q> {
    automaton: A,
    inputs: Vec<String>,
    outputs: Vec<String>,
    statuses: Vec<AtomicPropositionStatus>,
    game: LabelledGame<AutomatonTreeLabel>,
    queue: Q,
    stats: ExplorationStats,
}

impl<A: MaxEvenDPA, Q: ExplorationQueue<NodeIndex, A::EdgeLabel>> GameConstructor<A, Q>
where
    A::EdgeLabel: Clone + Eq + Ord,
{
    const SYS_OWNER: Player = Player::Even;
    const ENV_OWNER: Player = Player::Odd;
    const LEAF_OWNER: Player = Self::SYS_OWNER;

    pub(crate) fn new(automaton_spec: AutomatonSpecification<A>, mut queue: Q) -> Self {
        let initial_label =
            AutomatonTreeLabel::new(automaton_spec.automaton.initial_state(), TreeIndex::ROOT);
        let mut game = LabelledGame::default();
        let (initial_node, _) = game.add_border_node(initial_label);
        game.set_initial_node(initial_node);
        queue.push(initial_node);

        Self {
            automaton: automaton_spec.automaton,
            inputs: automaton_spec.inputs,
            outputs: automaton_spec.outputs,
            statuses: automaton_spec.statuses,
            game,
            queue,
            stats: ExplorationStats::default(),
        }
    }

    fn add_successor(
        queue: &mut Q,
        game: &mut LabelledGame<AutomatonTreeLabel>,
        node_index: NodeIndex,
        label: AutomatonTreeLabel,
        score_option: Option<A::EdgeLabel>,
    ) {
        let (successor_index, new_node) = game.add_border_node(label);
        game.add_edge(node_index, successor_index);
        if new_node {
            if let Some(score) = score_option {
                queue.push_scored(successor_index, score);
            } else {
                queue.push(successor_index);
            }
        }
    }

    pub(crate) fn explore(&mut self, limit: ExplorationLimit) {
        let split = self.inputs.len();
        let start = Instant::now();
        let mut explored_states = 0;
        let mut explored_edges = 0;
        let mut explored_nodes = 0;
        while let Some(node_index) = self.queue.pop() {
            let label = self.game[node_index].label();
            let state = label.automaton_state();
            let tree_index = label.tree_index();
            let tree = self.automaton.successors(state);
            if tree_index == TreeIndex::ROOT {
                explored_states += 1;
            }
            explored_nodes += 1;

            // update node information and add successors
            match &tree[tree_index] {
                TreeNode::Inner(node) => {
                    let env = node.var() < split;
                    let target_var = env.then(|| split);
                    let owner = if env {
                        Self::ENV_OWNER
                    } else {
                        Self::SYS_OWNER
                    };
                    self.game.update_node(node_index, owner, Color::default());
                    for tree_succ_index in tree.index_iter(tree_index, target_var) {
                        Self::add_successor(
                            &mut self.queue,
                            &mut self.game,
                            node_index,
                            AutomatonTreeLabel::new(state, tree_succ_index),
                            None,
                        );
                    }
                }
                TreeNode::Leaf(edge) => {
                    explored_edges += 1;
                    self.game
                        .update_node(node_index, Self::LEAF_OWNER, edge.color());
                    let successor_state = edge.successor();
                    Self::add_successor(
                        &mut self.queue,
                        &mut self.game,
                        node_index,
                        AutomatonTreeLabel::new(successor_state, TreeIndex::ROOT),
                        Some(edge.label().clone()),
                    );
                }
            };

            if match limit {
                ExplorationLimit::None => false,
                ExplorationLimit::Nodes(n) => explored_nodes >= n,
                ExplorationLimit::Edges(n) => explored_edges >= n,
                ExplorationLimit::States(n) => explored_states >= n,
                ExplorationLimit::Time(n) => start.elapsed() >= n,
            } {
                break;
            }
        }
        let new_stats = ExplorationStats::new(
            explored_states,
            explored_edges,
            explored_nodes,
            start.elapsed(),
        );
        self.stats += new_stats;
    }
}

impl<A: MaxEvenDPA, Q> GameConstructor<A, Q> {
    pub(crate) fn get_game(&self) -> &LabelledGame<AutomatonTreeLabel> {
        &self.game
    }

    pub(crate) fn stats(&self) -> &ExplorationStats {
        &self.stats
    }

    pub(crate) fn into_game(self) -> LabelledGame<AutomatonTreeLabel> {
        self.game
    }

    pub(crate) fn into_mealy_machine(
        self,
        winner: Player,
        strategy: Strategy,
    ) -> (LabelledMachine<StateIndex>, A) {
        let machine = MealyConstructor::construct(
            &self.automaton,
            self.inputs,
            self.outputs,
            self.statuses,
            self.game,
            strategy,
            winner,
        );
        (machine, self.automaton)
    }
}

pub(crate) struct MealyConstructor<'a, A: MaxEvenDPA + 'a> {
    input_manager: Cudd,
    output_manager: Cudd,
    automaton: &'a A,
    inputs: Vec<String>,
    outputs: Vec<String>,
    game: LabelledGame<AutomatonTreeLabel>,
    strategy: Strategy,
    mealy: bool,
    input_status_bdd: BDD,
    output_status_bdd: BDD,
}

impl<'a, A: MaxEvenDPA + 'a> MealyConstructor<'a, A> {
    fn leaf_successor(&self, node_index: NodeIndex) -> NodeIndex {
        self.game[node_index].successors()[0]
    }

    fn successors(
        &'a self,
        node_index_arr: &'a [NodeIndex; 1],
        use_strategy: bool,
        player: Player,
    ) -> &'a [NodeIndex] {
        let node_index = node_index_arr[0];
        let node = &self.game[node_index];
        let state_index = node.label().automaton_state();
        let tree_index = node.label().tree_index();
        if node.owner() != player
            || self.automaton.edge_tree(state_index).unwrap()[tree_index].is_leaf()
        {
            node_index_arr
        } else if use_strategy {
            &self.strategy[node_index]
        } else {
            node.successors()
        }
    }

    fn get_bdd(&self, source: NodeIndex, target: NodeIndex, input: bool) -> BDD {
        let source_node = &self.game[source];
        let target_node = &self.game[target];
        let source_state_index = source_node.label().automaton_state();
        let target_state_index = target_node.label().automaton_state();
        assert_eq!(source_state_index, target_state_index);
        let source_tree_index = source_node.label().tree_index();
        let target_tree_index = target_node.label().tree_index();

        let edge_tree = self.automaton.edge_tree(source_state_index).unwrap();
        if input {
            edge_tree.bdd_for_paths(
                &self.input_manager,
                source_tree_index,
                target_tree_index,
                Some(self.inputs.len()),
                0,
            ) & &self.input_status_bdd
        } else {
            edge_tree.bdd_for_paths(
                &self.output_manager,
                source_tree_index,
                target_tree_index,
                None,
                -(self.inputs.len() as isize),
            ) & &self.output_status_bdd
        }
    }

    pub(crate) fn construct(
        automaton: &A,
        inputs: Vec<String>,
        outputs: Vec<String>,
        statuses: Vec<AtomicPropositionStatus>,
        game: LabelledGame<AutomatonTreeLabel>,
        strategy: Strategy,
        winner: Player,
    ) -> LabelledMachine<StateIndex> {
        let mealy = winner == Player::Even;
        let num_inputs = inputs.len();
        let num_outputs = outputs.len();

        let input_manager = Cudd::with_vars(num_inputs).unwrap();
        let output_manager = Cudd::with_vars(num_outputs).unwrap();
        // compute status BDDs
        let mut input_status_bdd = input_manager.bdd_one();
        let mut output_status_bdd = output_manager.bdd_one();
        for (var, status) in statuses.into_iter().enumerate() {
            if !mealy && var < num_inputs {
                match status {
                    AtomicPropositionStatus::True => input_status_bdd &= input_manager.bdd_var(var),
                    AtomicPropositionStatus::False => {
                        input_status_bdd &= !input_manager.bdd_var(var)
                    }
                    _ => (),
                }
            } else if mealy && var >= num_inputs {
                match status {
                    AtomicPropositionStatus::True => {
                        output_status_bdd &= output_manager.bdd_var(var - num_inputs)
                    }
                    AtomicPropositionStatus::False => {
                        output_status_bdd &= !output_manager.bdd_var(var - num_inputs)
                    }
                    _ => (),
                }
            }
        }

        let constructor = MealyConstructor {
            input_manager,
            output_manager,
            automaton,
            inputs,
            outputs,
            game,
            strategy,
            mealy,
            input_status_bdd,
            output_status_bdd,
        };
        constructor.construct_internal()
    }

    fn construct_internal(self) -> LabelledMachine<StateIndex> {
        let mut m = LabelledMachineConstructor::new();

        let mut queue = VecDeque::new();
        let initial_node = self.game.initial_node();
        let initial_label = self.game[initial_node].label().automaton_state();
        let (initial_state, _) = m.add_state(initial_label);
        queue.push_back((initial_node, initial_state));

        while let Some((node_index, state_index)) = queue.pop_front() {
            for &input_successor in self.successors(&[node_index], !self.mealy, Player::Odd) {
                let input = self.get_bdd(node_index, input_successor, true);
                let mut transition = Transition::new(input);
                for &output_successor in
                    self.successors(&[input_successor], self.mealy, Player::Even)
                {
                    let output = self.get_bdd(input_successor, output_successor, false);
                    let successor_index = self.leaf_successor(output_successor);

                    let successor_node = &self.game[successor_index];
                    assert_eq!(successor_node.label().tree_index(), TreeIndex::ROOT);
                    let (successor_state, new_state) =
                        m.add_state(successor_node.label().automaton_state());

                    transition.add_output(output, successor_state);

                    if new_state {
                        queue.push_back((successor_index, successor_state));
                    }
                }
                m.add_transition(state_index, transition);
            }
        }
        m.into_machine(initial_state, self.inputs, self.outputs, self.mealy)
    }
}
