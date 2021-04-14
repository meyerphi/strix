//! Automata for ω-words.

use std::convert::TryFrom;
use std::os::raw::{c_double, c_int, c_void};

use ordered_float::NotNan;

use crate::bindings::*;
use crate::formula::Ltl;
use crate::graal::Vm;
use crate::tree::{Node, TreeIndex, ValuationTree};

/// An index for a state of an automaton.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct StateIndex(isize);

impl std::fmt::Display for StateIndex {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if self == &Self::TOP {
            write!(f, "⊤")
        } else if self == &Self::BOTTOM {
            write!(f, "⊥")
        } else {
            write!(f, "{}", self.0)
        }
    }
}

impl Ord for StateIndex {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        // order TOP and BOTTOM before ordinary state indices
        self.0.cmp(&other.0)
    }
}

impl PartialOrd for StateIndex {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl StateIndex {
    /// Index for the top sink state, from which every word is accepted.
    pub const TOP: Self = Self(-2);
    /// Index for the bottom sink state, from which no word is accepted.
    pub const BOTTOM: Self = Self(-1);

    /// Returns true if this is the index for the top or bottom sink state.
    pub fn is_sink(self) -> bool {
        self == Self::TOP || self == Self::BOTTOM
    }

    /// Trys conversion of a value into a state index.
    ///
    /// Note: due to the blanket implementation for `TryFrom` in the standard
    /// library, we cannot implement the `TryFrom` trait directly.
    fn try_from<I>(value: I) -> Result<Self, <isize as TryFrom<I>>::Error>
    where
        isize: TryFrom<I>,
    {
        Ok(Self(isize::try_from(value)?))
    }
}

/// The color of an edge of an automaton.
pub type Color = usize;

/// An edge of an automaton.
#[derive(Copy, Clone, Debug)]
pub struct Edge<L> {
    /// The index of the successor state.
    successor: StateIndex,
    /// The color of the edge.
    color: Color,
    /// The label of the edge.
    label: L,
}

impl<L> Edge<L> {
    /// Creates a new edge with the given succcessor, color and label.
    const fn new(successor: StateIndex, color: Color, label: L) -> Self {
        Self {
            successor,
            color,
            label,
        }
    }

    /// The index of the successor state of the edge.
    pub const fn successor(&self) -> StateIndex {
        self.successor
    }

    /// The color of the edge.
    pub const fn color(&self) -> Color {
        self.color
    }

    /// The label of the edge.
    pub const fn label(&self) -> &L {
        &self.label
    }
}

/// A tree containing the successor edges of a state for each valuation.
pub type EdgeTree<L> = ValuationTree<Edge<L>>;

/// A deterministic parity automaton with max-even acceptance, i.e.
/// a word (a sequence of valuations) is accepted if and only if
/// the maximal color along the unique run of the word is even.
pub trait MaxEvenDpa {
    /// The type of label for edges.
    type EdgeLabel: std::fmt::Debug;

    /// The initial state of the DPA.
    fn initial_state(&self) -> StateIndex;
    /// The number of colors used in the DPA. This should be at least
    /// one higher than the maximal color on any edge.
    fn num_colors(&self) -> Color;
    /// Computes the successors at the state with given index, and returns
    /// the edge tree of successors.
    fn successors(&mut self, state: StateIndex) -> &EdgeTree<Self::EdgeLabel>;
    /// Returns the edge tree of successors at the state with the given index,
    /// if it has been computed before.
    fn edge_tree(&self, state: StateIndex) -> Option<&EdgeTree<Self::EdgeLabel>>;
    /// Decomposes the state with the given index into structured labels.
    fn decompose(&self, state: StateIndex) -> Vec<i32>;
}

/// The acceptance condition of an automaton returned by Owl.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
enum AcceptanceCondition {
    /// Safety acceptance: a run is accepting iff the bottom sink state is not reached.
    Safety,
    /// Co-safety acceptance: a run is accepting iff the top sink state is reached.
    CoSafety,
    /// Büchi acceptance: a run is accepting iff a colored edge is seen infinitely often.
    Buchi,
    /// Co-Büchi acceptance: a run is accepting iff a colored edge is only seen finitely often.
    CoBuchi,
    /// Parity max-even acceptance: a run is accepting iff the maximum color seen infinitely often
    /// is even.
    ParityMaxEven,
    /// Parity max-odd acceptance: a run is accepting iff the maximum color seen infinitely often
    /// is odd.
    ParityMaxOdd,
    /// Parity min-even acceptance: a run is accepting iff the minimum color seen infinitely often
    /// is even.
    ParityMinEven,
    /// Parity min-odd acceptance: a run is accepting iff the minimum color seen infinitely often
    /// is odd.
    ParityMinOdd,
}

/// Information about the acceptance condition of the underlying Owl automaton.
#[derive(Copy, Clone)]
struct AutomatonInfo {
    /// The acceptance condition of the Owl automaton.
    acceptance: AcceptanceCondition,
    /// The number of colors, already adjusted for max-even parity acceptance.
    num_colors: Color,
}

impl AutomatonInfo {
    /// Creates the automaton information from the values given by Owl.
    fn from_owl(acc: acceptance_t, acc_sets: c_int) -> Self {
        let acceptance = Self::convert_acceptance(acc);
        let num_colors = Self::init_num_colors(acceptance, acc_sets);
        assert!(num_colors >= 1);
        Self {
            acceptance,
            num_colors,
        }
    }

    /// Converts the acceptance condition from the Owl enum.
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

    /// Initializes the number of colors for the automaton. The maximum
    /// color is adjusted so that the number is directly usable in a max-even DPA.
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

/// The edge label of an Owl automaton:
/// a score assigning successors a heuristical "trueness" value.
/// The value is guaranteed to be in range `0.0..=1.0`.
type Score = NotNan<f64>;

/// An max-even parity automaton constructed by Owl.
pub struct Automaton<'a> {
    /// The used GraalVM.
    vm: &'a Vm,
    /// The raw pointer to the automaton object.
    automaton: *mut c_void,
    /// Information about the acceptance of the automaton.
    info: AutomatonInfo,
    /// The successors of the automaton and whether they are already computed.
    successors: Vec<Option<EdgeTree<Score>>>,
}

impl<'a> Drop for Automaton<'a> {
    fn drop(&mut self) {
        unsafe { destroy_object_handle(self.vm.thread, self.automaton) };
    }
}

impl<'a> Automaton<'a> {
    /// Initializes the successor vector for the fixed top and bottom sink states.
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

    /// Creates an automaton for the given LTL formula, with optional simplification.
    pub fn of(vm: &'a Vm, formula: &Ltl, simplify_formula: bool) -> Self {
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

impl<'a> MaxEvenDpa for Automaton<'a> {
    type EdgeLabel = Score;

    fn initial_state(&self) -> StateIndex {
        StateIndex(0)
    }

    fn num_colors(&self) -> Color {
        self.info.num_colors
    }

    fn successors(&mut self, state: StateIndex) -> &EdgeTree<Score> {
        /// Converts the edge from Owl with the given acceptance information.
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
                AcceptanceCondition::CoSafety | AcceptanceCondition::CoBuchi => 1,
                AcceptanceCondition::Buchi => 2,
            };
            assert!(new_color < info.num_colors);
            assert!((0.0..=1.0).contains(&score));
            let new_score = Score::new(score).unwrap();
            Edge::new(new_successor, new_color, new_score)
        }

        /// Converts the index into the valuation tree from Owl.
        fn convert_tree_index(offset: usize, index: c_int) -> TreeIndex {
            let tree_index = if index < 0 {
                usize::try_from(-index).unwrap() - 1 + offset
            } else {
                usize::try_from(index).unwrap() / 3
            };
            TreeIndex(tree_index)
        }

        /// Computes the edge tree by querying the Owl automaton for the given state.
        fn compute_edge_tree(
            vm: &Vm,
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
                Node::new_inner(
                    usize::try_from(var).unwrap(),
                    convert_tree_index(num_nodes, left),
                    convert_tree_index(num_nodes, right),
                )
            }));
            tree.extend((0..num_edges).map(|i| {
                let successor = unsafe { *c_edges.elements.add(2 * i) };
                let color = unsafe { *c_edges.elements.add(2 * i + 1) };
                let score = unsafe { *c_scores.elements.add(i) };
                Node::new_leaf(convert_edge(info, successor, color, score))
            }));
            let edge_tree = EdgeTree::new_unchecked(tree);
            unsafe {
                free_unmanaged_memory(vm.thread, c_tree.elements as *mut _);
                free_unmanaged_memory(vm.thread, c_edges.elements as *mut _);
                free_unmanaged_memory(vm.thread, c_scores.elements as *mut _);
            }
            edge_tree
        }

        assert!(state.0 >= -2);
        let state_index = (state.0 + 2) as usize;

        if state_index >= self.successors.len() {
            self.successors.resize(state_index + 1, None)
        }

        // split up self for correct borrows
        let successors = &mut self.successors;
        let vm = self.vm;
        let automaton = self.automaton;
        let info = self.info;
        successors[state_index].get_or_insert_with(|| compute_edge_tree(vm, automaton, info, state))
    }

    fn edge_tree(&self, state: StateIndex) -> Option<&EdgeTree<Score>> {
        assert!(state.0 >= -2);
        let state_index = (state.0 + 2) as usize;
        self.successors
            .get(state_index)
            .map(Option::as_ref)
            .flatten()
    }

    fn decompose(&self, state: StateIndex) -> Vec<i32> {
        vec![state.0 as i32]
    }
}
