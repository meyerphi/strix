use std::fmt::{self, Display};
use std::str::FromStr;

use clap::{ArgGroup, Clap, Error, ErrorKind};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum InputFormat {
    LTL,
}

impl Default for InputFormat {
    fn default() -> Self {
        Self::LTL
    }
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum OutputFormat {
    #[clap(name = "pg")]
    PG,
    #[clap(name = "hoa")]
    HOA,
    #[clap(name = "bdd")]
    BDD,
    #[clap(name = "aag")]
    AAG,
    #[clap(name = "aig")]
    AIG,
}
impl Display for OutputFormat {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::PG => "pg",
                Self::HOA => "hoa",
                Self::BDD => "bdd",
                Self::AAG => "aag",
                Self::AIG => "aig",
            }
        )
    }
}
impl Default for OutputFormat {
    fn default() -> Self {
        Self::HOA
    }
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum LabelStructure {
    #[clap(name = "none")]
    None,
    #[clap(name = "outer")]
    Outer,
    #[clap(name = "inner")]
    Inner,
}

impl Default for LabelStructure {
    fn default() -> Self {
        Self::None
    }
}
impl Display for LabelStructure {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::None => "none",
                Self::Outer => "outer",
                Self::Inner => "inner",
            }
        )
    }
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum ExplorationStrategy {
    #[clap(name = "bfs")]
    BFS,
    #[clap(name = "dfs")]
    DFS,
    #[clap(name = "min")]
    Min,
    #[clap(name = "max")]
    Max,
    #[clap(name = "minmax")]
    MinMax,
}
impl Display for ExplorationStrategy {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::BFS => "bfs",
                Self::DFS => "dfs",
                Self::Min => "min",
                Self::Max => "max",
                Self::MinMax => "minmax",
            }
        )
    }
}
impl Default for ExplorationStrategy {
    fn default() -> Self {
        Self::BFS
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum ScoringFunction {
    Default,
}
impl Display for ScoringFunction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::Default => "default",
            }
        )
    }
}
impl Default for ScoringFunction {
    fn default() -> Self {
        Self::Default
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OnTheFlyLimit {
    None,
    Nodes(usize),
    Edges(usize),
    States(usize),
    Seconds(u64),
    TimeMultiple(u32),
}
impl Display for OnTheFlyLimit {
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
impl FromStr for OnTheFlyLimit {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s.is_empty() {
            return Err(Error::with_description(
                "".to_string(),
                ErrorKind::EmptyValue,
            ));
        }
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
                Err(Error::with_description(
                    format!(
                        "invalid number '{}' for value 'none' [must be empty]",
                        number
                    ),
                    ErrorKind::ValueValidation,
                ))
            }
        } else if !matches!(value, "n" | "e" | "s" | "t" | "m") {
            Err(Error::with_description(
                format!(
                    "invalid value '{}' [possible values: none, n<num>, e<num>, s<num>, t<num>, m<num>]",
                    value
                ),
                ErrorKind::InvalidValue,
            ))
        } else if number.is_empty() {
            Err(Error::with_description(
                format!("no number for value '{}'", value),
                ErrorKind::ValueValidation,
            ))
        } else {
            let num = number.parse::<u64>().map_err(|e| {
                Error::with_description(
                    format!("could not parse number '{}': {}", number, e),
                    ErrorKind::ValueValidation,
                )
            })?;
            const LIMIT: u64 = 1 << 16;
            if num == 0 || num >= LIMIT {
                Err(Error::with_description(
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
impl Default for OnTheFlyLimit {
    fn default() -> Self {
        Self::TimeMultiple(20)
    }
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum Solver {
    #[clap(name = "fpi")]
    FPI,
    #[clap(name = "zlk")]
    ZLK,
    #[clap(name = "si")]
    SI,
}
impl Display for Solver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::FPI => "fpi",
                Self::ZLK => "zlk",
                Self::SI => "si",
            }
        )
    }
}
impl Default for Solver {
    fn default() -> Self {
        Self::FPI
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum Simplification {
    #[clap(name = "none")]
    None,
    #[clap(name = "language")]
    Language,
    #[clap(name = "realizability")]
    Realizability,
}
impl Display for Simplification {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::None => "none",
                Self::Language => "language",
                Self::Realizability => "realizability",
            }
        )
    }
}
impl Default for Simplification {
    fn default() -> Self {
        Self::Realizability
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum MinimizationMethod {
    #[clap(name = "none")]
    None,
    #[clap(name = "nd")]
    NonDeterminism,
    #[clap(name = "dc")]
    DontCares,
    #[clap(name = "both")]
    Both,
}
impl Display for MinimizationMethod {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::None => "none",
                Self::NonDeterminism => "nd",
                Self::DontCares => "dc",
                Self::Both => "both",
            }
        )
    }
}
impl Default for MinimizationMethod {
    fn default() -> Self {
        Self::None
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum AigerCompression {
    #[clap(name = "none")]
    None,
    #[clap(name = "basic")]
    Basic,
    #[clap(name = "more")]
    More,
}
impl Display for AigerCompression {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::None => "none",
                Self::Basic => "basic",
                Self::More => "more",
            }
        )
    }
}
impl Default for AigerCompression {
    fn default() -> Self {
        Self::More
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum BddReordering {
    #[clap(name = "none")]
    None,
    #[clap(name = "heuristic")]
    Heuristic,
    #[clap(name = "mixed")]
    Mixed,
    #[clap(name = "exact")]
    Exact,
}
impl Display for BddReordering {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::None => "none",
                Self::Heuristic => "heuristic",
                Self::Mixed => "mixed",
                Self::Exact => "exact",
            }
        )
    }
}
impl Default for BddReordering {
    fn default() -> Self {
        Self::Mixed
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Clap)]
pub enum TraceLevel {
    #[clap(name = "off")]
    Off,
    #[clap(name = "error")]
    Error,
    #[clap(name = "warn")]
    Warn,
    #[clap(name = "info")]
    Info,
    #[clap(name = "debug")]
    Debug,
    #[clap(name = "trace")]
    Trace,
}
impl Display for TraceLevel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::Off => "off",
                Self::Error => "error",
                Self::Warn => "warn",
                Self::Info => "info",
                Self::Debug => "debug",
                Self::Trace => "trace",
            }
        )
    }
}
impl Default for TraceLevel {
    fn default() -> Self {
        Self::Error
    }
}

impl Into<log::LevelFilter> for TraceLevel {
    fn into(self) -> log::LevelFilter {
        match self {
            Self::Off => log::LevelFilter::Off,
            Self::Error => log::LevelFilter::Error,
            Self::Warn => log::LevelFilter::Warn,
            Self::Info => log::LevelFilter::Info,
            Self::Debug => log::LevelFilter::Debug,
            Self::Trace => log::LevelFilter::Trace,
        }
    }
}

#[derive(Debug, Clone, Default, Clap)]
#[clap(version = env!("CARGO_PKG_VERSION"), about)]
#[clap(group = ArgGroup::new("input-formula").required(true))]
pub struct Options {
    #[clap(
        short = 'r',
        long = "realizability",
        about = "Only check realizability",
        display_order = 0
    )]
    pub only_realizability: bool,
    #[clap(
        short = 'a',
        long = "aiger",
        about = "Use portfolio approach to construct small aiger ciruit",
        display_order = 1
    )]
    pub aiger_portfolio: bool,
    #[clap(
        short = 'd',
        long = "determinize",
        about = "Determinize controller automaton",
        display_order = 2
    )]
    pub machine_determinization: bool,
    #[clap(
        long = "filter",
        about = "Use reachable state filter during exploration",
        display_order = 4
    )]
    pub exploration_filter: bool,
    #[clap(
        short = 'f',
        long = "formula",
        about = "LTL formula of the specification",
        group = "input-formula",
        display_order = 0
    )]
    pub formula: Option<String>,
    #[clap(
        short = 'F',
        long = "formula-file",
        about = "Read LTL formula from the the given file",
        group = "input-formula",
        display_order = 1
    )]
    pub input_file: Option<String>,
    #[clap(
        long = "ins",
        about = "Comma-separated list of input proposition",
        use_delimiter = true,
        min_values = 0,
        display_order = 2
    )]
    pub inputs: Vec<String>,
    #[clap(
        long = "outs",
        about = "Comma-separated list of output proposition",
        use_delimiter = true,
        min_values = 0,
        display_order = 3
    )]
    pub outputs: Vec<String>,
    #[clap(skip)]
    pub input_format: InputFormat,
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
    #[clap(
        short = 'O',
        long = "output-file",
        about = "Write controller to the given file",
        display_order = 5
    )]
    pub output_file: Option<String>,
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
    #[clap(
        arg_enum,
        long = "scoring",
        name = "scoring-function",
        default_value,
        about = "Scoring function to use for min/max/minmax strategy",
        display_order = 7
    )]
    pub exploration_scoring: ScoringFunction,
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
        arg_enum,
        short = 's',
        long = "solver",
        name = "parity-solver",
        default_value,
        about = "Parity game solver to use",
        display_order = 9
    )]
    pub parity_solver: Solver,
    #[clap(
        arg_enum,
        long = "simplification",
        name = "ltl-level",
        default_value,
        about = "Level of LTL simplification (none, with language or with realizability equivalence)",
        display_order = 10
    )]
    pub ltl_simplification: Simplification,
    #[clap(
        arg_enum,
        short = 'm',
        long = "minimize",
        name = "method",
        default_value,
        about = "Method for minimization of automaton (minimize number of states using non-determinism (nd) and/or don't-cares (dc)",
        display_order = 11
    )]
    pub machine_minimization: MinimizationMethod,
    #[clap(
        arg_enum,
        short = 'l',
        long = "label",
        name = "structure",
        default_value,
        about = "Label structure to use",
        display_order = 12
    )]
    pub label_structure: LabelStructure,
    #[clap(
        arg_enum,
        long = "reordering",
        name = "bdd-strategy",
        default_value,
        about = "BDD reordering strategy",
        display_order = 13
    )]
    pub bdd_reordering: BddReordering,
    #[clap(
        arg_enum,
        long = "compression",
        name = "aig-strategy",
        default_value,
        about = "Aiger compression strategy",
        display_order = 14
    )]
    pub aiger_compression: AigerCompression,
    #[clap(
        arg_enum,
        short = 't',
        long = "trace",
        name = "trace-level",
        default_value,
        about = "Trace level",
        display_order = 15
    )]
    pub trace_level: TraceLevel,
}

#[derive(Debug, Clone, Default)]
pub struct SynthesisOptions {
    pub output_format: OutputFormat,
    pub only_realizability: bool,
    pub aiger_portfolio: bool,
    pub exploration_scoring: ScoringFunction,
    pub exploration_strategy: ExplorationStrategy,
    pub exploration_filter: bool,
    pub exploration_on_the_fly: OnTheFlyLimit,
    pub parity_solver: Solver,
    pub machine_determinization: bool,
    pub machine_minimization: MinimizationMethod,
    pub label_structure: LabelStructure,
    pub ltl_simplification: Simplification,
    pub bdd_reordering: BddReordering,
    pub aiger_compression: AigerCompression,
}

impl From<&Options> for SynthesisOptions {
    fn from(options: &Options) -> Self {
        Self {
            output_format: options.output_format,
            only_realizability: options.only_realizability,
            aiger_portfolio: options.aiger_portfolio,
            exploration_scoring: options.exploration_scoring,
            exploration_strategy: options.exploration_strategy,
            exploration_filter: options.exploration_filter,
            exploration_on_the_fly: options.exploration_on_the_fly,
            parity_solver: options.parity_solver,
            machine_determinization: options.machine_determinization,
            machine_minimization: options.machine_minimization,
            label_structure: options.label_structure,
            ltl_simplification: options.ltl_simplification,
            bdd_reordering: options.bdd_reordering,
            aiger_compression: options.aiger_compression,
        }
    }
}
