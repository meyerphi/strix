use std::convert::TryFrom;
use std::os::raw::{c_double, c_int, c_void};

use ordered_float::NotNan;

use crate::bindings::*;
use crate::formula::LTLFormula;
use crate::graal::GraalVM;
use crate::tree::{TreeIndex, TreeNode, ValuationTree};
use crate::{Color, Edge, StateIndex};

pub type EdgeTree<L> = ValuationTree<Edge<L>>;

pub trait MaxEvenDPA {
    type EdgeLabel: std::fmt::Debug;

    fn initial_state(&self) -> StateIndex;
    fn num_colors(&self) -> Color;
    fn successors(&mut self, state: StateIndex) -> &EdgeTree<Self::EdgeLabel>;
    fn edge_tree(&self, state: StateIndex) -> &EdgeTree<Self::EdgeLabel>;
    fn decompose(&self, state: StateIndex) -> Vec<i32>;
}

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
enum AcceptanceCondition {
    Safety,
    CoSafety,
    Buchi,
    CoBuchi,
    ParityMaxEven,
    ParityMaxOdd,
    ParityMinEven,
    ParityMinOdd,
}

#[derive(Copy, Clone)]
struct AutomatonInfo {
    acceptance: AcceptanceCondition,
    num_colors: Color,
}

impl AutomatonInfo {
    fn from_owl(acc: acceptance_t, acc_sets: c_int) -> AutomatonInfo {
        let acceptance = Self::convert_acceptance(acc);
        let num_colors = Self::init_num_colors(acceptance, acc_sets);
        assert!(num_colors >= 1);
        AutomatonInfo {
            acceptance,
            num_colors,
        }
    }

    fn convert_acceptance(acc: acceptance_t) -> AcceptanceCondition {
        #![allow(non_upper_case_globals)]
        match acc {
            acceptance_t_SAFETY => AcceptanceCondition::Safety,
            acceptance_t_CO_SAFETY => AcceptanceCondition::CoSafety,
            acceptance_t_BUCHI => AcceptanceCondition::Buchi,
            acceptance_t_CO_BUCHI => AcceptanceCondition::CoBuchi,
            acceptance_t_PARITY_MAX_EVEN => AcceptanceCondition::ParityMaxEven,
            acceptance_t_PARITY_MAX_ODD => AcceptanceCondition::ParityMaxOdd,
            acceptance_t_PARITY_MIN_EVEN => AcceptanceCondition::ParityMinEven,
            acceptance_t_PARITY_MIN_ODD => AcceptanceCondition::ParityMinOdd,
            _ => panic!("Unsupported acceptance condition: {}", acc),
        }
    }

    fn init_num_colors(a: AcceptanceCondition, acc_sets: c_int) -> Color {
        let d = Color::try_from(acc_sets).unwrap();
        match a {
            AcceptanceCondition::ParityMaxEven => d,
            AcceptanceCondition::ParityMaxOdd => d + 1,
            AcceptanceCondition::ParityMinEven => d + (1 - (d % 2)),
            AcceptanceCondition::ParityMinOdd => d + (d % 2),
            AcceptanceCondition::Safety
            | AcceptanceCondition::CoSafety
            | AcceptanceCondition::CoBuchi => 2,
            AcceptanceCondition::Buchi => 3,
        }
    }
}

type Score = NotNan<f64>;

pub struct Automaton<'a> {
    vm: &'a GraalVM,
    automaton: *mut c_void,
    info: AutomatonInfo,
    successors: Vec<Option<EdgeTree<Score>>>,
}

impl<'a> Drop for Automaton<'a> {
    fn drop(&mut self) {
        unsafe { destroy_object_handle(self.vm.thread, self.automaton) };
    }
}

impl<'a> Automaton<'a> {
    fn init_successors() -> Vec<Option<EdgeTree<Score>>> {
        let mut successors = Vec::with_capacity(4096);

        // top state in vec index 0 => lookup index -2
        assert_eq!(StateIndex::TOP.0, -2);
        successors.push(Some(EdgeTree::single(Edge::new(
            StateIndex::TOP,
            0,
            Score::new(1.0).unwrap(),
        ))));
        // bottom state in vec index 1 => lookup index -1
        assert_eq!(StateIndex::BOTTOM.0, -1);
        successors.push(Some(EdgeTree::single(Edge::new(
            StateIndex::BOTTOM,
            1,
            Score::new(0.0).unwrap(),
        ))));

        successors
    }

    pub fn of(vm: &'a GraalVM, formula: &LTLFormula, simplify_formula: bool) -> Self {
        let automaton = unsafe {
            if simplify_formula {
                automaton_of2(
                    vm.thread,
                    formula.formula,
                    ltl_to_dpa_translation_t_UNPUBLISHED_ZIELONKA,
                    ltl_translation_option_t_SIMPLIFY_FORMULA,
                    ltl_translation_option_t_USE_PORTFOLIO_FOR_SYNTACTIC_LTL_FRAGMENTS,
                )
            } else {
                automaton_of1(
                    vm.thread,
                    formula.formula,
                    ltl_to_dpa_translation_t_UNPUBLISHED_ZIELONKA,
                    ltl_translation_option_t_USE_PORTFOLIO_FOR_SYNTACTIC_LTL_FRAGMENTS,
                )
            }
        };
        let acc = unsafe { automaton_acceptance_condition(vm.thread, automaton) };
        let acc_sets = unsafe { automaton_acceptance_condition_sets(vm.thread, automaton) };
        let info = AutomatonInfo::from_owl(acc, acc_sets);
        let successors = Self::init_successors();
        Automaton {
            vm,
            automaton,
            info,
            successors,
        }
    }
}

impl<'a> MaxEvenDPA for Automaton<'a> {
    type EdgeLabel = Score;

    fn initial_state(&self) -> StateIndex {
        StateIndex(0)
    }

    fn num_colors(&self) -> Color {
        self.info.num_colors
    }

    fn successors(&mut self, state: StateIndex) -> &EdgeTree<Score> {
        assert!(state.0 >= -2);
        let state_index = (state.0 + 2) as usize;

        if state_index >= self.successors.len() {
            self.successors.resize(state_index + 1, None)
        }

        fn convert_edge(
            info: AutomatonInfo,
            successor: c_int,
            color: c_int,
            score: c_double,
        ) -> Edge<Score> {
            let new_successor = StateIndex::try_from(successor).unwrap();
            let new_color = match info.acceptance {
                AcceptanceCondition::ParityMaxEven
                | AcceptanceCondition::ParityMaxOdd
                | AcceptanceCondition::ParityMinEven
                | AcceptanceCondition::ParityMinOdd
                | AcceptanceCondition::CoBuchi
                    if color == -1 =>
                {
                    0
                }
                AcceptanceCondition::Buchi if color == -1 => 1,
                // turn parity into max even parity
                AcceptanceCondition::ParityMaxEven => Color::try_from(color).unwrap(),
                AcceptanceCondition::ParityMaxOdd => Color::try_from(color).unwrap() + 1,
                AcceptanceCondition::ParityMinEven | AcceptanceCondition::ParityMinOdd => {
                    info.num_colors - 1 - Color::try_from(color).unwrap()
                }
                AcceptanceCondition::Safety => 0,
                AcceptanceCondition::CoSafety => 1,
                AcceptanceCondition::Buchi => 2,
                AcceptanceCondition::CoBuchi => 1,
            };
            assert!(new_color < info.num_colors);
            let new_score = Score::new(score).unwrap();
            Edge::new(new_successor, new_color, new_score)
        }

        fn convert_tree_index(offset: usize, index: c_int) -> TreeIndex {
            let tree_index = if index < 0 {
                usize::try_from(-index).unwrap() - 1 + offset
            } else {
                usize::try_from(index).unwrap() / 3
            };
            TreeIndex(tree_index)
        }

        fn compute_edge_tree(
            vm: &GraalVM,
            automaton: *mut c_void,
            info: AutomatonInfo,
            state: StateIndex,
        ) -> EdgeTree<Score> {
            let mut c_tree = vector_int_t {
                elements: std::ptr::null_mut(),
                size: 0,
            };
            let mut c_edges = vector_int_t {
                elements: std::ptr::null_mut(),
                size: 0,
            };
            let mut c_scores = vector_double_t {
                elements: std::ptr::null_mut(),
                size: 0,
            };
            unsafe {
                automaton_edge_tree(
                    vm.thread,
                    automaton,
                    state.0 as c_int,
                    &mut c_tree,
                    &mut c_edges,
                    &mut c_scores,
                );
            }
            assert_eq!(c_edges.size % 2, 0);
            assert_eq!(c_edges.size, 2 * c_scores.size);
            assert_eq!(c_tree.size % 3, 0);

            let num_nodes = (c_tree.size as usize) / 3;
            let num_edges = (c_edges.size as usize) / 2;

            let mut tree = Vec::with_capacity(num_nodes + num_edges);
            tree.extend((0..num_nodes).map(|i| {
                let var = unsafe { *c_tree.elements.add(3 * i) };
                let left = unsafe { *c_tree.elements.add(3 * i + 1) };
                let right = unsafe { *c_tree.elements.add(3 * i + 2) };
                TreeNode::new_node(
                    usize::try_from(var).unwrap(),
                    convert_tree_index(num_nodes, left),
                    convert_tree_index(num_nodes, right),
                )
            }));
            tree.extend((0..num_edges).map(|i| {
                let successor = unsafe { *c_edges.elements.add(2 * i) };
                let color = unsafe { *c_edges.elements.add(2 * i + 1) };
                let score = unsafe { *c_scores.elements.add(i) };
                TreeNode::new_leaf(convert_edge(info, successor, color, score))
            }));
            let edge_tree = EdgeTree::new_unchecked(tree);
            unsafe {
                free_unmanaged_memory(vm.thread, c_tree.elements as *mut _);
                free_unmanaged_memory(vm.thread, c_edges.elements as *mut _);
                free_unmanaged_memory(vm.thread, c_scores.elements as *mut _);
            }
            edge_tree
        }

        // split up self for correct borrows
        let successors = &mut self.successors;
        let vm = self.vm;
        let automaton = self.automaton;
        let info = self.info;
        successors[state_index].get_or_insert_with(|| compute_edge_tree(vm, automaton, info, state))
    }

    fn edge_tree(&self, state: StateIndex) -> &EdgeTree<Score> {
        assert!(state.0 >= -2);
        let state_index = (state.0 + 2) as usize;
        &self.successors[state_index].as_ref().unwrap()
    }

    fn decompose(&self, state: StateIndex) -> Vec<i32> {
        vec![state.0 as i32]
    }
}
