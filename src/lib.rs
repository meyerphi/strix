//! Strix library crate for reactive synthesis of controllers from LTL specifications.

mod constructor;
pub mod controller;
pub mod options;
pub mod parity;

use std::fmt::{self, Display};
use std::time::Duration;

use log::{debug, info, trace, warn};
use owl::automaton::{MaxEvenDpa, StateIndex};
use owl::formula::AtomicPropositionStatus;

use constructor::queue::{BfsQueue, DfsQueue, ExplorationQueue, MinMaxMode, MinMaxQueue};
use constructor::{AutomatonSpecification, ExplorationLimit, GameConstructor};
use controller::aiger::AigerController;
use controller::bdd::BddController;
use controller::labelling::{
    AutomatonLabelling, AutomatonTreeLabel, SimpleLabelling, StructuredLabel,
};
use controller::machine::LabelledMachine;
use options::{
    AigerCompression, BddReordering, ExplorationStrategy, LabelCompression, LabelStructure,
    MinimizationMethod, OnTheFlyLimit, OutputFormat, Simplification, Solver, SynthesisOptions,
};
use parity::game::{LabelledGame, NodeIndex, Player};
use parity::solver::{
    FpiSolver, IncrementalParityGameSolver, IncrementalSolver, ParityGameSolver, SiSolver,
    ZlkSolver,
};

/// The realizability status for a specification.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum Status {
    /// The specification is realizable.
    Realizable,
    /// The specification is unrealizable.
    Unrealizable,
}

impl From<Player> for Status {
    fn from(player: Player) -> Self {
        match player {
            Player::Even => Self::Realizable,
            Player::Odd => Self::Unrealizable,
        }
    }
}

impl From<Status> for Player {
    fn from(status: Status) -> Self {
        match status {
            Status::Realizable => Self::Even,
            Status::Unrealizable => Self::Odd,
        }
    }
}

impl Display for Status {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::Realizable => "REALIZABLE",
                Self::Unrealizable => "UNREALIZABLE",
            }
        )
    }
}

/// Synthesize an LTL specification with the given LTL formula, list of input
/// atomic propositions and list of atomic output propositions.
///
/// Returns the result of the synthesis procedure. This function uses the default
/// values for [`SynthesisOptions`].
pub fn synthesize(ltl: &str, ins: &[&str], outs: &[&str]) -> SynthesisResult {
    synthesize_with(ltl, ins, outs, &SynthesisOptions::default())
}

/// Synthesize an LTL specification with the given LTL formula, list of input
/// atomic propositions and list of atomic output propositions, using the
/// given synthesis options.
///
/// Returns the result of the synthesis procedure.
pub fn synthesize_with(
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    options: &SynthesisOptions,
) -> SynthesisResult {
    let num_inputs = ins.len();
    let num_outputs = outs.len();

    let mut ap = Vec::with_capacity(num_inputs + num_outputs);
    ap.extend_from_slice(ins);
    ap.extend_from_slice(outs);

    let vm = owl::graal::Vm::new().unwrap();
    let mut formula = owl::formula::Ltl::parse(&vm, ltl, &ap);
    debug!("Parsed formula: {}", formula);
    let statuses = if options.ltl_simplification == Simplification::Realizability {
        info!("Applying realizability simplifications");
        formula.simplify(num_inputs, num_outputs)
    } else {
        vec![AtomicPropositionStatus::Used; num_inputs + num_outputs]
    };
    debug!("Simplified formula: {}", formula);
    for (&status, &a) in statuses.iter().zip(ap.iter()) {
        match status {
            AtomicPropositionStatus::Unused => {
                warn!("Atomic proposition {} not used formula", a)
            }
            AtomicPropositionStatus::True => warn!(
                "Atomic proposition {} only used positively, may be replaced with true",
                a
            ),
            AtomicPropositionStatus::False => warn!(
                "Atomic proposition {} only used negatively, may be replaced with false",
                a
            ),
            AtomicPropositionStatus::Used => (),
        }
    }
    info!("Creating automaton");
    let automaton = owl::automaton::Automaton::of(
        &vm,
        &formula,
        options.ltl_simplification == Simplification::Language,
    );
    info!("Finished creating automaton");

    let automaton_spec = AutomatonSpecification::new(automaton, ins, outs, statuses);
    match options.exploration_strategy {
        ExplorationStrategy::Bfs => {
            explore_with(BfsQueue::with_capacity(4096), automaton_spec, options)
        }
        ExplorationStrategy::Dfs => {
            explore_with(DfsQueue::with_capacity(4096), automaton_spec, options)
        }
        ExplorationStrategy::Min => explore_with(
            MinMaxQueue::with_capacity(4096, MinMaxMode::Min),
            automaton_spec,
            options,
        ),
        ExplorationStrategy::Max => explore_with(
            MinMaxQueue::with_capacity(4096, MinMaxMode::Max),
            automaton_spec,
            options,
        ),
        ExplorationStrategy::MinMax => explore_with(
            MinMaxQueue::with_capacity(4096, MinMaxMode::MinMax),
            automaton_spec,
            options,
        ),
    }
}

/// A controller for a specification.
pub enum Controller {
    /// The parity game from which realizability or unrealizability of the specification
    /// was determined.
    ///
    /// This is not an actual controller, but the template for a controller. The labels
    /// of the nodes of the parity game refer to the indices of nodes in edge trees for
    /// states of the automaton from which the game was constructed.
    ParityGame(LabelledGame<AutomatonTreeLabel>),
    /// A controller in form of a Mealy or Moore machine for the specification or its negation.
    Machine(LabelledMachine<StructuredLabel>),
    /// A controller in form of a BDD.
    Bdd(BddController),
    /// A controller in form of an aiger circuit.
    Aiger(AigerController),
}

impl Controller {
    /// Writes the controller to the given writer.
    /// The given status is used for completing the border if the controller is a parity game.
    /// The binary flag is used to control the output if the controller is an aiger circuit.
    ///
    /// # Errors
    ///
    /// Returns an error if an I/O error occurs during the write operation.
    pub fn write<W: std::io::Write>(
        &self,
        mut writer: W,
        status: Status,
        binary: bool,
    ) -> std::io::Result<()> {
        match self {
            Self::ParityGame(game) => game.write_with_winner(writer, Player::from(status)),
            Self::Machine(machine) => write!(writer, "{}", machine),
            Self::Bdd(bdd) => write!(writer, "{}", bdd),
            Self::Aiger(aiger) => aiger.write(writer, binary),
        }
    }
}

/// A result of the synthesis procedure.
pub struct SynthesisResult {
    /// The realizability status for the specification.
    status: Status,
    /// A controller for the specification, if a controller has been produced.
    controller: Option<Controller>,
}

impl SynthesisResult {
    /// Returns the realizability status for the specification in this result.
    pub fn status(&self) -> Status {
        self.status
    }

    /// Returns the controller for the specification in this result, if a controller
    /// has been produced.
    pub fn controller(&self) -> &Option<Controller> {
        &self.controller
    }

    fn only_status(status: Status) -> Self {
        Self {
            status,
            controller: None,
        }
    }
    fn with_game(status: Status, game: LabelledGame<AutomatonTreeLabel>) -> Self {
        Self {
            status,
            controller: Some(Controller::ParityGame(game)),
        }
    }
    fn with_machine(status: Status, machine: LabelledMachine<StructuredLabel>) -> Self {
        Self {
            status,
            controller: Some(Controller::Machine(machine)),
        }
    }
    fn with_bdd(status: Status, bdd: BddController) -> Self {
        Self {
            status,
            controller: Some(Controller::Bdd(bdd)),
        }
    }
    fn with_aiger(status: Status, aiger: AigerController) -> Self {
        Self {
            status,
            controller: Some(Controller::Aiger(aiger)),
        }
    }
}

fn explore_with<A: MaxEvenDpa, Q: ExplorationQueue<NodeIndex, A::EdgeLabel>>(
    queue: Q,
    automaton_spec: AutomatonSpecification<A>,
    options: &SynthesisOptions,
) -> SynthesisResult
where
    A::EdgeLabel: Clone + Eq + Ord,
{
    let constructor = GameConstructor::new(automaton_spec, queue);

    match options.parity_solver {
        Solver::Fpi => solve_with(constructor, FpiSolver::new(), options),
        Solver::Zlk => solve_with(constructor, ZlkSolver::new(), options),
        Solver::Si => solve_with(constructor, SiSolver::new(), options),
    }
}

fn solve_with<A: MaxEvenDpa, Q: ExplorationQueue<NodeIndex, A::EdgeLabel>, S: ParityGameSolver>(
    mut constructor: GameConstructor<A, Q>,
    solver: S,
    options: &SynthesisOptions,
) -> SynthesisResult
where
    A::EdgeLabel: Clone + Eq + Ord,
{
    info!("Exploring automaton and solving game");
    let mut limit = match options.exploration_on_the_fly {
        OnTheFlyLimit::None => ExplorationLimit::None,
        OnTheFlyLimit::Nodes(n) => ExplorationLimit::Nodes(n),
        OnTheFlyLimit::Edges(n) => ExplorationLimit::Edges(n),
        OnTheFlyLimit::States(n) => ExplorationLimit::States(n),
        OnTheFlyLimit::Seconds(n) => ExplorationLimit::Time(Duration::from_secs(n)),
        OnTheFlyLimit::TimeMultiple(_) => ExplorationLimit::Time(Duration::from_secs(0)),
    };

    let mut incremental_solver = IncrementalSolver::new(solver);
    loop {
        constructor.explore(limit);
        let game = constructor.get_game();
        let result = incremental_solver.solve(game);
        let construction_stats = constructor.stats();
        let solver_stats = incremental_solver.stats();

        trace!("Stats: {}; {}", construction_stats, solver_stats);

        if let Some(winner) = result {
            info!("Game solved, winner is {}", winner);
            return construct_result(winner, constructor, incremental_solver, options);
        }

        // dynamically scale exploration limit for time multiple option
        if let OnTheFlyLimit::TimeMultiple(n) = options.exploration_on_the_fly {
            limit = ExplorationLimit::Time(
                (solver_stats.time() * n)
                    .checked_sub(construction_stats.time())
                    .unwrap_or_else(Duration::default),
            );
        }
    }
}

fn construct_result<
    A: MaxEvenDpa,
    Q: ExplorationQueue<NodeIndex, A::EdgeLabel>,
    S: ParityGameSolver,
>(
    winner: Player,
    constructor: GameConstructor<A, Q>,
    mut solver: IncrementalSolver<S>,
    options: &SynthesisOptions,
) -> SynthesisResult
where
    A::EdgeLabel: Clone + Eq + Ord,
{
    let status = Status::from(winner);
    if options.output_format == OutputFormat::Pg {
        let game = constructor.into_game();
        SynthesisResult::with_game(status, game)
    } else if options.only_realizability {
        SynthesisResult::only_status(status)
    } else {
        info!("Obtaining winning strategy");
        let strategy = solver.strategy(constructor.get_game(), winner);
        let construction_stats = constructor.stats();
        let solver_stats = solver.stats();
        trace!("Stats: {}; {}", construction_stats, solver_stats);

        info!("Constructing machine");
        let (machine, automaton) = constructor.into_mealy_machine(winner, strategy);
        construct_result_from_machine(status, machine, &automaton, options)
    }
}

fn construct_result_from_machine<A: MaxEvenDpa>(
    status: Status,
    mut machine: LabelledMachine<StateIndex>,
    automaton: &A,
    options: &SynthesisOptions,
) -> SynthesisResult
where
    A::EdgeLabel: Clone + Eq + Ord,
{
    let mut min_machine = None;

    // avoid minimization in portfolio approach for very large machines
    let min_portfolio = options.aiger_portfolio && machine.num_states() <= 4000;
    let min_nondet = min_portfolio
        || matches!(
            options.machine_minimization,
            MinimizationMethod::NonDeterminism | MinimizationMethod::Both
        );
    let min_dontcare = min_portfolio
        || matches!(
            options.machine_minimization,
            MinimizationMethod::DontCares | MinimizationMethod::Both
        );

    let compress_features = matches!(
        options.label_compression,
        LabelCompression::Features | LabelCompression::Both
    );

    if min_nondet {
        machine = machine.minimize_with_nondeterminism();
    }
    if min_dontcare {
        machine.determinize();
        min_machine = Some(machine.minimize_with_dontcares(compress_features));
    }

    // machines needs to be deterministic for other output formats
    if options.machine_determinization
        || (!min_dontcare && options.output_format != OutputFormat::Hoa)
    {
        machine.determinize();
    }

    // add labels
    let mut structured_machines = Vec::new();
    if options.aiger_portfolio {
        if let Some(min_machine) = min_machine {
            if min_machine.num_states() < machine.num_states() {
                let m0 = min_machine.with_structured_labels(&mut SimpleLabelling::default());
                structured_machines.push(m0);
                let m1 =
                    min_machine.with_structured_labels(&mut AutomatonLabelling::new(automaton));
                structured_machines.push(m1);
            }
        }
        let m2 = machine.with_structured_labels(&mut SimpleLabelling::default());
        let m3 = machine.with_structured_labels(&mut AutomatonLabelling::new(automaton));
        structured_machines.push(m2);
        structured_machines.push(m3);
        // TODO add inner structure
    } else if let Some(min_machine) = min_machine {
        let m = match options.label_structure {
            LabelStructure::None => {
                min_machine.with_structured_labels(&mut SimpleLabelling::default())
            }
            LabelStructure::Structured => {
                min_machine.with_structured_labels(&mut AutomatonLabelling::new(automaton))
            }
        };
        structured_machines.push(m);
    } else {
        let m = match options.label_structure {
            LabelStructure::None => machine.with_structured_labels(&mut SimpleLabelling::default()),
            LabelStructure::Structured => {
                machine.with_structured_labels(&mut AutomatonLabelling::new(automaton))
            }
        };
        structured_machines.push(m);
    }

    construct_result_from_structured_machines(status, structured_machines, options)
}

fn construct_result_from_structured_machines(
    status: Status,
    mut structured_machines: Vec<LabelledMachine<StructuredLabel>>,
    options: &SynthesisOptions,
) -> SynthesisResult {
    if options.output_format == OutputFormat::Hoa {
        SynthesisResult::with_machine(status, structured_machines.remove(0))
    } else {
        let mut bdds: Vec<_> = structured_machines
            .into_iter()
            .map(|m| m.create_bdds())
            .collect();

        for bdd in &mut bdds {
            match options.bdd_reordering {
                BddReordering::Heuristic => bdd.reduce(false),
                BddReordering::Mixed => bdd.reduce(bdd.num_bdd_vars() <= 16),
                BddReordering::Exact => bdd.reduce(true),
                BddReordering::None => (),
            };
        }

        if options.output_format == OutputFormat::Bdd {
            SynthesisResult::with_bdd(status, bdds.remove(0))
        } else {
            let mut aigs: Vec<_> = bdds.into_iter().map(|bdd| bdd.create_aiger()).collect();
            // in portfolio approach, skip compressing circuits relatively much larger than old minimum
            let min_size = aigs.iter().map(AigerController::size).min().unwrap();
            let min_size_total = min_size.total() as f32;
            let cmp_size = min_size_total + (min_size_total * 10000.0) / (min_size_total + 1000.0);
            for aig in &mut aigs {
                if !options.aiger_portfolio || (aig.size().total() as f32) <= cmp_size {
                    match options.aiger_compression {
                        AigerCompression::Basic => aig.compress(false),
                        AigerCompression::More => aig.compress(true),
                        AigerCompression::None => (),
                    };
                }
            }
            assert!(matches!(
                options.output_format,
                OutputFormat::Aag | OutputFormat::Aig
            ));
            SynthesisResult::with_aiger(
                status,
                aigs.into_iter().min_by_key(AigerController::size).unwrap(),
            )
        }
    }
}
