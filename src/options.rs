//! Options for the synthesis procedure.

use std::fmt;
use std::str::FromStr;

use clap::{ArgGroup, Clap, Error, ErrorKind};

/// Implement [`Display`](std::fmt::Display) with the information in [`clap::ArgEnum`].
///
/// This ensures consistent names for parsing of the default argument.
macro_rules! clap_display {
    ($t:ty) => {
        impl std::fmt::Display for $t
        where
            $t: clap::ArgEnum,
        {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                use clap::ArgEnum as _;
                let self_str = Self::VARIANTS
                    .iter()
                    .find(|s| &Self::from_str(s, false).unwrap() == self)
                    .unwrap();
                write!(f, "{}", self_str)
            }
        }
    };
}

/// The input format of the specification.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum InputFormat {
    /// A specification in linear temporal logic (LTL).
    Ltl,
}
impl Default for InputFormat {
    fn default() -> Self {
        Self::Ltl
    }
}
clap_display!(InputFormat);

/// The output format for the controller.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum OutputFormat {
    /// Parity game output.
    #[clap(name = "pg")]
    Pg,
    /// Machine controller in HOA format.
    #[clap(name = "hoa")]
    Hoa,
    /// Controller as a binary decision diagram (BDD).
    #[clap(name = "bdd")]
    Bdd,
    /// Controller as an aiger circuit in ASCII format.
    #[clap(name = "aag")]
    Aag,
    /// Controller as an aiger circuit in binary format.
    #[clap(name = "aig")]
    Aig,
}
impl Default for OutputFormat {
    fn default() -> Self {
        Self::Hoa
    }
}
clap_display!(OutputFormat);

/// The type of labels used in the machine controller
/// for further translation to a BDD or aiger circuit.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum LabelStructure {
    /// No structure. This will use the state index
    /// of a machine state as the label.
    #[clap(name = "none")]
    None,
    /// Structured labels derived from the states
    /// of the parity automaton for the machine.
    #[clap(name = "structured")]
    Structured,
}
impl Default for LabelStructure {
    fn default() -> Self {
        Self::None
    }
}
clap_display!(LabelStructure);

/// The method to compress structured labels in a machine
/// by reducing the number of features or number of values.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum LabelCompression {
    /// Do not compress labels.
    #[clap(name = "none")]
    None,
    /// Reduce the number of features for the labels.
    #[clap(name = "features")]
    Features,
    /// Reduce the number of values for each label feature.
    #[clap(name = "values")]
    Values,
    /// Combine reduction of features and values,
    /// first applying [`LabelCompression::Features`]
    /// and then [`LabelCompression::Values`].
    #[clap(name = "both")]
    Both,
}
impl Default for LabelCompression {
    fn default() -> Self {
        Self::None
    }
}
clap_display!(LabelCompression);

/// The strategy to use for choosing the next node in
/// the parity game during on-the-fly exploration.
///
/// The min, max and minmax strategies use a scoring
/// of nodes derived from states of the parity automaton.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum ExplorationStrategy {
    /// Explore nodes in a breadth-first search, i.e.
    /// choose the node that was discovered the earliest as the next node.
    #[clap(name = "bfs")]
    Bfs,
    /// Explore nodes in a depth-first search, i.e.
    /// choose the node that was discovered the latest as the next node.
    #[clap(name = "dfs")]
    Dfs,
    /// Explore nodes by choosing the node with the minimum score
    /// as the next node.
    #[clap(name = "min")]
    Min,
    /// Explore nodes by choosing the node with the maximum score
    /// as the next node.
    #[clap(name = "max")]
    Max,
    /// Explore nodes by alternatingly choosing the node with the
    /// minimum and maximum score next.
    #[clap(name = "minmax")]
    MinMax,
}
impl Default for ExplorationStrategy {
    fn default() -> Self {
        Self::Bfs
    }
}
clap_display!(ExplorationStrategy);

/// The scoring function to use during on-the-fly exploration
/// with an exploration strategy that uses scores.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum ScoringFunction {
    /// The default scoring function of the automaton.
    #[clap(name = "default")]
    Default,
}
impl Default for ScoringFunction {
    fn default() -> Self {
        Self::Default
    }
}
clap_display!(ScoringFunction);

/// Option that controls the number of nodes that are
/// explored in each step of the on-the-fly exploration
/// before the parity game solver is called.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OnTheFlyLimit {
    /// No limit. This means all nodes are explored before
    /// the solver is called, so on-the-fly exploration is turned off.
    None,
    /// Explore the given number of parity game nodes before the
    /// solver is called.
    Nodes(usize),
    /// Explore the given number of edges of the parity automaton
    /// before the solver is called.
    Edges(usize),
    /// Explore the given number of states of the parity automaton
    /// before the solver is called.
    States(usize),
    /// Let exploration run for the given number of seconds until the
    /// solver is called. This method does not interrupt the exploration
    /// and waits until exploration of the current node finishes, so in
    /// each step at least one node is explored.
    Seconds(u64),
    /// Let exploration run until the total exploration time is at least
    /// equal or greater to the total solver time so far, multiplied with
    /// the given number.
    ///
    /// For instance, if this option is used with the value 10, then
    /// the solver time will approximately be 10% of the exploration time.
    TimeMultiple(u32),
}
impl Default for OnTheFlyLimit {
    fn default() -> Self {
        Self::TimeMultiple(20)
    }
}
impl fmt::Display for OnTheFlyLimit {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::None => write!(f, "none"),
            Self::Nodes(n) => write!(f, "n{}", n),
            Self::Edges(n) => write!(f, "e{}", n),
            Self::States(n) => write!(f, "s{}", n),
            Self::Seconds(n) => write!(f, "t{}", n),
            Self::TimeMultiple(n) => write!(f, "m{}", n),
        }
    }
}

/// An error which can be returned when parsing an on-the-fly limit.
#[derive(Debug)]
pub struct ParseOnTheFlyLimitError {
    msg: String,
    kind: ErrorKind,
}
impl ParseOnTheFlyLimitError {
    fn new(msg: String, kind: ErrorKind) -> Self {
        Self { msg, kind }
    }
    fn to_clap_error(&self) -> Error {
        Error::with_description(self.msg.clone(), self.kind)
    }
}
impl fmt::Display for ParseOnTheFlyLimitError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&self.to_clap_error(), f)
    }
}
impl std::error::Error for ParseOnTheFlyLimitError {}

impl FromStr for OnTheFlyLimit {
    type Err = ParseOnTheFlyLimitError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        // parse longest prefix until a number is encountered
        let split = s
            .char_indices()
            .find_map(|(i, c)| c.is_numeric().then(|| i))
            .unwrap_or_else(|| s.len());
        let value = &s[..split];
        let number = &s[split..];
        if value == "none" {
            if number.is_empty() {
                Ok(Self::None)
            } else {
                Err(ParseOnTheFlyLimitError::new(
                    format!(
                        "invalid number '{}' for value 'none' [must be empty]",
                        number
                    ),
                    ErrorKind::ValueValidation,
                ))
            }
        } else if !matches!(value, "n" | "e" | "s" | "t" | "m") {
            Err(ParseOnTheFlyLimitError::new(
                format!(
                    "invalid value '{}' [possible values: none, n<num>, e<num>, s<num>, t<num>, m<num>]",
                    value
                ),
                ErrorKind::InvalidValue,
            ))
        } else if number.is_empty() {
            Err(ParseOnTheFlyLimitError::new(
                format!("no number for value '{}'", value),
                ErrorKind::ValueValidation,
            ))
        } else {
            let num = number.parse::<u64>().map_err(|e| {
                ParseOnTheFlyLimitError::new(
                    format!("could not parse number '{}': {}", number, e),
                    ErrorKind::ValueValidation,
                )
            })?;
            const LIMIT: u64 = 1 << 16;
            if num == 0 || num >= LIMIT {
                Err(ParseOnTheFlyLimitError::new(
                    format!(
                        "number '{}' out of range [must be greater than 0 and less than {}]",
                        num, LIMIT
                    ),
                    ErrorKind::ValueValidation,
                ))
            } else {
                Ok(match value {
                    "n" => Self::Nodes(num as usize),
                    "e" => Self::Edges(num as usize),
                    "s" => Self::States(num as usize),
                    "t" => Self::Seconds(num as u64),
                    "m" => Self::TimeMultiple(num as u32),
                    _ => unreachable!(),
                })
            }
        }
    }
}

/// The algorithm to use for the parity game solver.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum Solver {
    /// Use fixed-point iteration (FPI).
    ///
    /// Described in:
    /// [Simple Fixpoint Iteration To Solve Parity Games](https://arxiv.org/abs/1909.07659),
    /// T. van Dijk and B. Rubbens, EPTCS 2019.
    #[clap(name = "fpi")]
    Fpi,
    /// Use Zielonka's recursive algorithm.
    ///
    /// Originally described in: [Infinite games on finitely coloured graphs with applications to automata on infinite trees](https://doi.org/10.1016/S0304-3975(98)00009-7),
    /// W. Zielonka, Theor. Comput. Sci., 1998.
    ///
    /// Uses optimizations from: [Oink: An Implementation and Evaluation of Modern Parity Game Solvers](https://doi.org/10.1007/978-3-319-89960-2_16),
    /// T. van Dijk, TACAS 2018.
    #[clap(name = "zlk")]
    Zlk,
    /// Use strategy iteration (SI).
    ///
    /// Described in:
    /// [Strategy Iteration using Non-Deterministic Strategies for Solving Parity Games](https://arxiv.org/abs/0806.2923),
    /// M. Luttenberger, 2012.
    #[clap(name = "si")]
    Si,
}
impl Default for Solver {
    fn default() -> Self {
        Self::Fpi
    }
}
clap_display!(Solver);

/// The simplications to apply to an LTL formula of the specification.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum Simplification {
    /// Apply no simplifications.
    #[clap(name = "none")]
    None,
    /// Apply simplifications preserving the language of the formula.
    #[clap(name = "language")]
    Language,
    /// Apply simplifications preserving realizability of the specification.
    #[clap(name = "realizability")]
    Realizability,
}
impl Default for Simplification {
    fn default() -> Self {
        Self::Realizability
    }
}
clap_display!(Simplification);

/// The minimization method to use on the controller machine.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum MinimizationMethod {
    /// Use no minimization.
    #[clap(name = "none")]
    None,
    /// Use a SAT-based minimization procedure that resolves
    /// non-determinism of successor states.
    #[clap(name = "nd")]
    NonDeterminism,
    /// Use a SAT-based minimization procedure that resolves
    /// "don't care" outputs.
    ///
    /// Described in:
    /// [MeMin: SAT-based Exact Minimization of Incompletely Specified Mealy Machines](http://embedded.cs.uni-saarland.de/MeMin.php),
    /// A. Abel and J. Reineke, ICCAD, 2015.
    ///
    /// This method first determinizes the machine heuristically such that there is no successor
    /// non-determinism and all output non-determinism is expressed using don't cares.
    #[clap(name = "dc")]
    DontCares,
    /// Combine both minimization methods, first applying [`MinimizationMethod::NonDeterminism`]
    /// and then[`MinimizationMethod::DontCares`].
    #[clap(name = "both")]
    Both,
}
impl Default for MinimizationMethod {
    fn default() -> Self {
        Self::None
    }
}
clap_display!(MinimizationMethod);

/// The method to use for aiger compression, i.e. reduction of the circuit size.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum AigerCompression {
    /// Use no compression.
    #[clap(name = "none")]
    None,
    /// Apply basic rewrite methods of the ABC framework until the size is is not further reduced.
    #[clap(name = "basic")]
    Basic,
    /// Apply both basic and newer rewrite methods of the ABC framework until the size is
    /// is not further reduced.
    #[clap(name = "more")]
    More,
}
impl Default for AigerCompression {
    fn default() -> Self {
        Self::More
    }
}
clap_display!(AigerCompression);

/// The method to use for reordering the BDD controller to reduce its size.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum BddReordering {
    /// Use no reordering.
    #[clap(name = "none")]
    None,
    /// Use the sift heuristic until convergence for reordering.
    #[clap(name = "heuristic")]
    Heuristic,
    /// Use [`BddReordering::Heuristic`] if the BDD has more than 16 variabes,
    /// and use [`BddReordering::Exact`] if the BDD has at most 16 variables.
    #[clap(name = "mixed")]
    Mixed,
    /// Use an exact dynamic-programming based method for reordering.
    #[clap(name = "exact")]
    Exact,
}
impl Default for BddReordering {
    fn default() -> Self {
        Self::Mixed
    }
}
clap_display!(BddReordering);

/// The trace level / verbosity for the logging framework
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum TraceLevel {
    /// Turn logging off.
    #[clap(name = "off")]
    Off,
    /// Only print errors.
    #[clap(name = "error")]
    Error,
    /// Print errors and warnings.
    #[clap(name = "warn")]
    Warn,
    /// Print errors, warnings and useful information.
    #[clap(name = "info")]
    Info,
    /// Print errors, warnings, useful and debug information.
    #[clap(name = "debug")]
    Debug,
    /// Print all information, including very verbose output.
    #[clap(name = "trace")]
    Trace,
}
impl Default for TraceLevel {
    fn default() -> Self {
        Self::Error
    }
}
clap_display!(TraceLevel);

impl From<TraceLevel> for log::LevelFilter {
    fn from(level: TraceLevel) -> Self {
        match level {
            TraceLevel::Off => Self::Off,
            TraceLevel::Error => Self::Error,
            TraceLevel::Warn => Self::Warn,
            TraceLevel::Info => Self::Info,
            TraceLevel::Debug => Self::Debug,
            TraceLevel::Trace => Self::Trace,
        }
    }
}
// Workaround for https://github.com/TeXitoi/structopt/issues/333
#[cfg_attr(not(doc), allow(missing_docs))]
#[cfg_attr(
    doc,
    doc = r#"
A group of options used for parsing the arguments of the
command-line interface.

This struct should mainly be used with [`clap`] and not
instantiated manually. For using this crate as library,
please use [`SynthesisOptions`] directly instead. This struct
only includes additional fields for specifying input
and output options.
"#
)]
#[derive(Debug, Clone, Default, Clap)]
#[clap(version, about)]
#[clap(group = ArgGroup::new("input-formula").required(true))]
pub struct CliOptions {
    /// The LTL formula for the specification.
    /// Either this field or [`CliOptions::input_file`] has to be set.
    #[clap(
        short = 'f',
        long = "formula",
        about = "LTL formula of the specification",
        group = "input-formula",
        display_order = 0
    )]
    pub formula: Option<String>,
    /// The input file from which the LTL formula for the specification is read.
    /// Either this field or [`CliOptions::formula`] has to be set.
    #[clap(
        short = 'F',
        long = "formula-file",
        about = "Read LTL formula from the the given file",
        group = "input-formula",
        display_order = 1
    )]
    pub input_file: Option<String>,
    /// The list of input atomic propositions for the specification.
    #[clap(
        long = "ins",
        about = "Comma-separated list of input proposition",
        use_delimiter = true,
        min_values = 0,
        display_order = 2
    )]
    pub inputs: Vec<String>,
    /// The list of output atomic propositions for the specification.
    #[clap(
        long = "outs",
        about = "Comma-separated list of output proposition",
        use_delimiter = true,
        min_values = 0,
        display_order = 3
    )]
    pub outputs: Vec<String>,
    /// The input format of the specification.
    #[clap(skip)]
    pub input_format: InputFormat,
    /// The output file where the controller should be written to.
    #[clap(
        short = 'O',
        long = "output-file",
        about = "Write controller to the given file",
        display_order = 5
    )]
    pub output_file: Option<String>,
    #[clap(
        arg_enum,
        short = 't',
        long = "trace",
        name = "trace-level",
        default_value,
        about = "Trace level",
        display_order = 17
    )]
    /// The trace level to use for instantiating the logging framework.
    pub trace_level: TraceLevel,
    /// The set of options for the synthesis process.
    #[clap(flatten)]
    pub synthesis_options: SynthesisOptions,
}

// Workaround for https://github.com/TeXitoi/structopt/issues/333
#[cfg_attr(not(doc), allow(missing_docs))]
#[cfg_attr(
    doc,
    doc = r#"
Options to control the synthesis procedure and the generation of the controller.

These options can then be used with [`synthesize_with`](crate::synthesize_with).

# Examples

```
use strix::options::*;
let options = SynthesisOptions {
    output_format: OutputFormat::Aag,
    machine_minimization: MinimizationMethod::DontCares,
    bdd_reordering: BddReordering::Exact,
    aiger_compression: AigerCompression::Basic,
    ..SynthesisOptions::default()
};
```
"#
)]
#[derive(Debug, Clone, Default, Clap)]
pub struct SynthesisOptions {
    /// Only check realizability of the specification.
    ///
    /// Setting this option to `true` results in an early return as soon
    /// as realizability is determined. Especially, no controller is produced,
    /// so many other synthesis option for the controller then become irrelevant.
    #[clap(
        short = 'r',
        long = "realizability",
        about = "Only check realizability",
        display_order = 0
    )]
    pub only_realizability: bool,
    /// Use a portfolio approach of machine minimization, structured labels and
    /// aiger compression to obtain a small aiger circuit.
    #[clap(
        short = 'a',
        long = "aiger",
        about = "Use portfolio approach to construct small aiger ciruit",
        display_order = 1
    )]
    pub aiger_portfolio: bool,
    /// The output format to use for the controller.
    #[clap(
        arg_enum,
        short = 'o',
        long = "output-format",
        name = "format",
        default_value,
        about = "Output format for controller (Parity Game, HOA automaton, BDD, AAG/AIG circuit)",
        display_order = 4
    )]
    pub output_format: OutputFormat,
    /// The scoring function to use for on-the-fly exploration.
    #[clap(
        arg_enum,
        long = "scoring",
        name = "scoring-function",
        default_value,
        about = "Scoring function to use for min/max/minmax strategy",
        display_order = 7
    )]
    pub exploration_scoring: ScoringFunction,
    /// The strategy to use for on-the-fly exploration.
    #[clap(
        arg_enum,
        short = 'e',
        long = "exploration",
        name = "exp-strategy",
        default_value,
        about = "On-the-fly exploration strategy",
        display_order = 6
    )]
    pub exploration_strategy: ExplorationStrategy,
    /// Filter unexplored states based on reachability from the inital state
    /// through non-winning states.
    #[clap(
        long = "filter",
        about = "Use reachable state filter during exploration",
        display_order = 4
    )]
    pub exploration_filter: bool,
    /// The limit to use for on-the-fly exploration.
    #[clap(
        long = "onthefly",
        name = "limit",
        default_value,
        about = "On-the-fly incremental exploration limit, where parity game solver is only invoked after:
    complete exploration [none]
    <num> new game nodes explored [n<num>]
    <num> new automaton edges explored [e<num>]
    <num> new automaton states explored [s<num>]
    <num> seconds spent in exploration [t<num>]
    <num> multiple of cumulative solver time [m<num>]\n",
        display_order = 8
    )]
    pub exploration_on_the_fly: OnTheFlyLimit,
    #[clap(
        long = "lookahead",
        name = "states",
        default_value,
        about = "Number of states that are explored ahead to determine \
        whether to apply the ACD or the Zielonka tree construction. \
        Use -1 to always apply the ACD, 0 to always apply the Zielonka tree, \
        and positive numbers to apply a mix of both.",
        display_order = 9
    )]
    pub lookahead: i32,
    /// The algorithm to use for the parity game solver.
    #[clap(
        arg_enum,
        short = 's',
        long = "solver",
        name = "parity-solver",
        default_value,
        about = "Parity game solver to use",
        display_order = 10
    )]
    pub parity_solver: Solver,
    /// Determinize the machine, i.e. ensure that there is a unique successor
    /// and a unique output only using don't cares for each input.
    ///
    /// If the output
    /// format is a BDD or an aiger circuit, or minimization using don't cares is
    /// enabled, then determinization is automatically enabled.
    #[clap(
        short = 'd',
        long = "determinize",
        about = "Determinize controller automaton",
        display_order = 2
    )]
    pub machine_determinization: bool,
    /// The minimization method to use for the machine.
    #[clap(
        arg_enum,
        short = 'm',
        long = "minimize",
        name = "method",
        default_value,
        about = "Method for minimization of automaton (minimize number of states using non-determinism (nd) and/or don't-cares (dc)",
        display_order = 12
    )]
    pub machine_minimization: MinimizationMethod,
    /// The type of structured labels that are used for the machine.
    #[clap(
        arg_enum,
        short = 'l',
        long = "label",
        name = "structure",
        default_value,
        about = "Label structure to use",
        display_order = 13
    )]
    pub label_structure: LabelStructure,
    /// The method for compressing structured labels.
    #[clap(
        arg_enum,
        long = "label-compression",
        name = "comp",
        default_value,
        about = "Label compression strategy to use",
        display_order = 14
    )]
    pub label_compression: LabelCompression,
    /// The method for simplication of the LTL formula.
    #[clap(
        arg_enum,
        long = "simplification",
        name = "ltl-level",
        default_value,
        about = "Level of LTL simplification (none, with language or with realizability equivalence)",
        display_order = 11
    )]
    pub ltl_simplification: Simplification,
    /// The method for reordering the BDD.
    #[clap(
        arg_enum,
        long = "reordering",
        name = "bdd-strategy",
        default_value,
        about = "BDD reordering strategy",
        display_order = 15
    )]
    pub bdd_reordering: BddReordering,
    /// The method for compressing the aiger circuit.
    #[clap(
        arg_enum,
        long = "compression",
        name = "aig-strategy",
        default_value,
        about = "Aiger compression strategy",
        display_order = 16
    )]
    pub aiger_compression: AigerCompression,
}

impl From<&CliOptions> for SynthesisOptions {
    fn from(options: &CliOptions) -> Self {
        options.synthesis_options.clone()
    }
}
