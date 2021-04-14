//! Integration test that verify controllers in AIGER or HOA format against
//! external model checkers.

use std::io::Write;
use std::path::PathBuf;
use std::process::Command;

use strix::options::*;
use strix::{
    synthesize_with, Controller,
    Status::{self, Realizable, Unrealizable},
};

/// Synthesize the given specification, only testing realizability,
/// and check the returned status against the expected status.
fn verify_realizability(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    verify_realizability_with(
        ltl,
        ins,
        outs,
        expected_status,
        &SynthesisOptions {
            only_realizability: true,
            ..SynthesisOptions::default()
        },
    );
}

/// Synthesize the given specification with the given options, only testing realizability,
/// and check the returned status against the expected status.
fn verify_realizability_with(
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    expected_status: Status,
    options: &SynthesisOptions,
) {
    let result = synthesize_with(ltl, ins, outs, options);
    assert_eq!(result.status(), expected_status);
}

/// Verify the given controller using the given script against the
/// given specification and status.
fn verify_controller<T: std::fmt::Display>(
    controller: T,
    script_file: &str,
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    status: Status,
) {
    let mut implementation_file = tempfile::NamedTempFile::new().unwrap();
    write!(implementation_file, "{}", controller).unwrap();

    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let script = root.join("scripts").join(script_file);

    let verification_status = Command::new(script)
        .arg(implementation_file.path())
        .arg(ltl)
        .arg(ins.join(","))
        .arg(outs.join(","))
        .arg(status.to_string())
        .status()
        .expect("failed to execute verification script");
    assert!(verification_status.success());
}

/// Synthesize the given specification, producing an aiger circuit, and verify
/// the circuit against the specification and given status.
fn verify_aiger(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    verify_aiger_with(
        ltl,
        ins,
        outs,
        expected_status,
        &SynthesisOptions {
            output_format: OutputFormat::Aag,
            aiger_portfolio: true,
            ..SynthesisOptions::default()
        },
    );
}

/// Synthesize the given specification with the given options, producing
/// an aiger circuit, and verify the circuit against the specification and given status.
///
/// The options should already have the output format set to `AAG` or `AIG`.
fn verify_aiger_with(
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    expected_status: Status,
    options: &SynthesisOptions,
) {
    let result = synthesize_with(ltl, ins, outs, options);
    assert_eq!(result.status(), expected_status);
    if let Some(Controller::Aiger(aiger)) = result.controller() {
        verify_controller(aiger, "verify_aiger.sh", ltl, ins, outs, expected_status);
    } else {
        panic!("no aiger controller produced");
    }
}

/// Synthesize the given specification, producing a machine in HOA format, and verify
/// the machine against the specification and given status.
fn verify_hoa(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    verify_hoa_with(
        ltl,
        ins,
        outs,
        expected_status,
        &SynthesisOptions {
            output_format: OutputFormat::Hoa,
            ..SynthesisOptions::default()
        },
    );
}

/// Synthesize the given specification with the given options, producing
/// a machine in HOA format, and verify the machine against the specification and given status.
///
/// The options should already have the output format set to `HOA`.
fn verify_hoa_with(
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    expected_status: Status,
    options: &SynthesisOptions,
) {
    let result = synthesize_with(ltl, ins, outs, options);
    assert_eq!(result.status(), expected_status);
    if let Some(Controller::Machine(machine)) = result.controller() {
        verify_controller(machine, "verify_hoa.sh", ltl, ins, outs, expected_status);
    } else {
        panic!("no machine controller produced");
    }
}

/// Synthesize the given specification, producing a parity game.
/// The parity game is currently *not* verified.
fn verify_pg(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    let options = SynthesisOptions {
        output_format: OutputFormat::Pg,
        ..SynthesisOptions::default()
    };
    let result = synthesize_with(ltl, ins, outs, &options);
    assert_eq!(result.status(), expected_status);
    // can not verify parity game itself currently
    assert!(matches!(
        result.controller(),
        Some(Controller::ParityGame(_))
    ));
}

/// Synthesize the given specification, producing a BDD controller.
/// The BDD is currently *not* verified.
fn verify_bdd(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    let options = SynthesisOptions {
        output_format: OutputFormat::Bdd,
        ..SynthesisOptions::default()
    };
    let result = synthesize_with(ltl, ins, outs, &options);
    assert_eq!(result.status(), expected_status);
    // can not verify BDD itself currently
    assert!(matches!(result.controller(), Some(Controller::Bdd(_))));
}

/// Generate tests for the given list of specifications, testing
/// realizability, aiger circuit synthesis and HOA machine synthesis.
macro_rules! synt_tests {
    ($($name:ident: ($ltl:expr, $ins:expr, $outs:expr, $expected_status:expr),)*) => {
        mod realizability {
            use super::*;
            $(
                #[test]
                fn $name() {
                    verify_realizability($ltl, $ins, $outs, $expected_status);
                }
            )*
        }

        mod aiger {
            use super::*;
            $(
                #[test]
                fn $name() {
                    verify_aiger($ltl, $ins, $outs, $expected_status);
                }
            )*
        }

        mod hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    verify_hoa($ltl, $ins, $outs, $expected_status);
                }
            )*
        }
    }
}

/// Generate tests for the given list of specifications, testing synthesis
/// with various synthesis options.
///
/// Generally, only one option is changed and the remaining set to their default.
/// Testing all combinations of options is currently infeasible.
macro_rules! option_tests {
    ($($name:ident: ($ltl:expr, $ins:expr, $outs:expr, $expected_status:expr),)*) => {
        mod exploration_bfs {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_strategy: ExplorationStrategy::Bfs,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod exploration_dfs {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_strategy: ExplorationStrategy::Dfs,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod exploration_min {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_strategy: ExplorationStrategy::Min,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod exploration_max {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_strategy: ExplorationStrategy::Max,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod exploration_minmax {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_strategy: ExplorationStrategy::MinMax,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod onthefly_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_on_the_fly: OnTheFlyLimit::None,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod onthefly_node1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_on_the_fly: OnTheFlyLimit::Nodes(1),
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod onthefly_edge1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_on_the_fly: OnTheFlyLimit::Edges(1),
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod onthefly_state1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_on_the_fly: OnTheFlyLimit::States(1),
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod onthefly_seconds1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_on_the_fly: OnTheFlyLimit::Seconds(1),
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod onthefly_multiple1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_on_the_fly: OnTheFlyLimit::TimeMultiple(1),
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod solver_si {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        parity_solver: Solver::Si,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod solver_dfi {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        parity_solver: Solver::Fpi,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod solver_zlk {
            use super::*;
            $(
                #[test]
                #[ignore] // ZLK does not yet provide a strategy
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        parity_solver: Solver::Zlk,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod simplification_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        ltl_simplification: Simplification::None,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod simplification_language {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        ltl_simplification: Simplification::Language,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod simplification_realizability {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        ltl_simplification: Simplification::Realizability,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod label_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        label_structure: LabelStructure::None,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod label_structured {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        label_structure: LabelStructure::Structured,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod label_compression_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        label_compression: LabelCompression::None,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod label_compression_features {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        label_compression: LabelCompression::Features,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod label_compression_values {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        label_compression: LabelCompression::Values,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod label_compression_both {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        label_compression: LabelCompression::Both,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod reordering_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        bdd_reordering: BddReordering::None,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod reordering_heuristic {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        bdd_reordering: BddReordering::Heuristic,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod reordering_mixed {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        bdd_reordering: BddReordering::Mixed,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod reordering_exact {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        bdd_reordering: BddReordering::Exact,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod compression_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        aiger_compression: AigerCompression::None,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod compression_basic {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        aiger_compression: AigerCompression::Basic,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod compression_more {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        aiger_compression: AigerCompression::More,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod output_pg {
            use super::*;
            $(
                #[test]
                fn $name() {
                    verify_pg($ltl, $ins, $outs, $expected_status);
                }
            )*
        }
        mod output_bdd {
            use super::*;
            $(
                #[test]
                fn $name() {
                    verify_bdd($ltl, $ins, $outs, $expected_status);
                }
            )*
        }
        mod output_aig {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aig,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod determinization_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Hoa,
                        machine_determinization: true,
                        ..SynthesisOptions::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_none_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Hoa,
                        machine_minimization: MinimizationMethod::None,
                        ..SynthesisOptions::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_nondet_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Hoa,
                        machine_minimization: MinimizationMethod::NonDeterminism,
                        ..SynthesisOptions::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_dontcares_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Hoa,
                        machine_minimization: MinimizationMethod::DontCares,
                        ..SynthesisOptions::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_both_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Hoa,
                        machine_minimization: MinimizationMethod::Both,
                        ..SynthesisOptions::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_none_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        machine_minimization: MinimizationMethod::None,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_nondet_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        machine_minimization: MinimizationMethod::NonDeterminism,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_dontcares_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        machine_minimization: MinimizationMethod::DontCares,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod minimization_both_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        machine_minimization: MinimizationMethod::Both,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod aiger_portfolio {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        aiger_portfolio: true,
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
        mod exploration_filter {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::Aag,
                        exploration_filter: true,
                        exploration_on_the_fly: OnTheFlyLimit::Nodes(1),
                        ..SynthesisOptions::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, &options);
                }
            )*
        }
    }
}

synt_tests! {
    ltl_true: ("true",  &[], &[], Realizable),
    ltl2dba01: ("(F (q & X (p U r))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba02: ("((p U (q U r)) & (q U (r U p)) & (r U (p U q))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba03: ("(F (p & X (q & X F r))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba04: ("(G (p -> (q U r))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba05: ("(p U (q & X (r U s))) <-> G F acc",  &["p", "q", "r", "s"], &["acc"] , Realizable),
    ltl2dba06: ("(F (p & X F (q & X F (r & X F s)))) <-> G F acc",  &["p", "q", "r", "s"], &["acc"] , Realizable),
    ltl2dba07: ("(p U (q & X (r & F (s & X F (u & X F (v & X F w)))))) <-> G F acc",  &["p", "q", "r", "s", "u", "v", "w"], &["acc"] , Realizable),
    ltl2dba08: ("((G F p) & (G F q) & (G F r) & (G F s) & (G F u)) <-> G F acc",  &["p", "q", "r", "s", "u"], &["acc"] , Realizable),
    ltl2dba09: ("((G F p) | (G F q) | (G F r)) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba10: ("(G (p -> F q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba11: ("(G (p U (q U (!p U !q)))) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba12: ("(((G p) -> F q) & ((G !p) -> F !q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba13: ("((G !r) & (G (p -> F q)) & (G (q -> F r))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba14: ("((G (p -> F q)) & (G r)) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba15: ("(G F (p -> X X X q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba16: ("((G (p -> F q)) & (G (!p -> F !q))) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba17: ("(G F (p <-> X X q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba18: ("((G (p -> F q)) & (G (q -> F r))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba19: ("(G (p -> X X X q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba20: ("((G (p -> F q)) & (G (r -> F s))) <-> G F acc",  &["p", "q", "r", "s"], &["acc"] , Realizable),
    ltl2dba21: ("(G F (p <-> X X X q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba22: ("(G p) <-> G F acc",  &["p"], &["acc"] , Realizable),
    ltl2dba23: ("(G (p -> G q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba24: ("(F p) <-> G F acc",  &["p"], &["acc"] , Realizable),
    ltl2dba25: ("(G (p & (q -> (q U (r & q))))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba26: ("(G (p -> G (q -> F r))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba_alpha_2: ("(F (p & X (p & X p) & (q & X (q & X q)))) <-> G F acc", &["p", "q"], &["acc"], Realizable),
    ltl2dba_beta_2: ("(F (p0 & F p1) & F (q0 & F q1)) <-> G F acc", &["p0", "p1", "q0", "q1"], &["acc"], Realizable),
    ltl2dba_c2_2: ("((G F p0) & (G F p1)) <-> G F acc", &["p0", "p1"], &["acc"], Realizable),
    ltl2dba_e_2: ("((F p0) & (F p1)) <-> G F acc", &["p0", "p1"], &["acc"], Realizable),
    ltl2dba_q_2: ("((F p0) | ((G p1) & (F p1))) <-> G F acc", &["p0", "p1"], &["acc"], Realizable),
    ltl2dba_u1_2: ("(p0 U p1) <-> G F acc", &["p0", "p1"], &["acc"], Realizable),
    unused_real: ("(a & !b & c & X !c) | (e & !f & g & X !g)",  &["a", "b", "c", "d"], &["e", "f", "g", "h"], Realizable),
    xg: ("a & X G !a", &[], &["a"], Realizable),
    xgf: ("(X G (a -> F b)) <-> G F acc", &["a", "b"], &["acc"], Realizable),
    amba_decode: ("G ((!\"HBURST_0\" & !\"HBURST_1\") -> \"SINGLE\") & G ((\"HBURST_0\" & !\"HBURST_1\") -> \"BURST4\") & G ((!\"HBURST_0\" & \"HBURST_1\") -> \"INCR\") & G !(\"SINGLE\" & (\"BURST4\" | \"INCR\")) & G !(\"BURST4\" & \"INCR\")", &["HBURST_0", "HBURST_1"], &["INCR", "BURST4", "SINGLE"], Realizable),
    amba_encode: ("(G (!\"HGRANT_0\" | !\"HGRANT_1\") & G (\"HGRANT_0\" | \"HGRANT_1\")) -> (G (\"HREADY\" -> ((X !\"HMASTER_0\") <-> \"HGRANT_0\")) & G (\"HREADY\" -> ((X \"HMASTER_0\") <-> \"HGRANT_1\")) & G (!\"HREADY\" -> ((X \"HMASTER_0\") <-> \"HMASTER_0\")))", &["HREADY", "HGRANT_0", "HGRANT_1"], &["HMASTER_0"], Realizable),
    amba_lock: ("(G (!\"HGRANT_0\" | !\"HGRANT_1\") & G (\"HGRANT_0\" | \"HGRANT_1\")) -> (G ((\"DECIDE\" & X \"HGRANT_0\") -> ((X \"LOCKED\") <-> X \"HLOCK_0\")) & G ((\"DECIDE\" & X \"HGRANT_1\") -> ((X \"LOCKED\") <-> X \"HLOCK_1\")) & G (!\"DECIDE\" -> ((X \"LOCKED\") <-> \"LOCKED\")))", &["DECIDE", "HLOCK_0", "HLOCK_1", "HGRANT_0", "HGRANT_1"], &["LOCKED"], Realizable),
    amba_shift: ("G (\"HREADY\" -> ((X \"HMASTLOCK\") <-> \"LOCKED\")) & G ((!\"HREADY\") -> ((X \"HMASTLOCK\") <-> \"HMASTLOCK\"))", &["LOCKED", "HREADY"], &["HMASTLOCK"], Realizable),
    amba_tsingle: ("(!\"DECIDE\" & (G F \"HREADY\") & G (!\"READY3\" -> X !\"DECIDE\")) -> (\"READY3\" & G (\"DECIDE\" -> X X (((\"SINGLE\" & \"LOCKED\") -> (!\"READY3\" U (\"HREADY\" & !\"READY3\"))) & (!(\"SINGLE\" & \"LOCKED\") -> \"READY3\"))) & G ((\"READY3\" & X !\"DECIDE\") -> X \"READY3\") & G ((\"READY3\" & X \"DECIDE\") -> X (!\"READY3\" & X !\"READY3\")))", &["DECIDE", "LOCKED", "HREADY", "SINGLE"], &["READY3"], Realizable),

    ltl_false: ("false",  &[], &[], Unrealizable),
    ltl2dba27: ("(F G !p) <-> G F acc",  &["p"], &["acc"], Unrealizable),
    ltl2dba_r_2: ("((G F p0) | ((F G p1) & (G F p1))) <-> G F acc", &["p0", "p1"], &["acc"], Unrealizable),
    ltl2dba_theta_2: ("!((G F p0) & (G F p1) & G (q -> F r)) <-> G F acc", &["r", "q", "p0", "p1"], &["acc"], Unrealizable),
    lilydemo01: ("G (req -> X (grant & X (grant & X grant))) & G (grant -> X !grant) & G (cancel -> X (!grant U go))",  &["go", "cancel", "req"], &["grant"], Unrealizable),
    lilydemo02: ("G (req -> X (grant | X (grant | X grant))) & G (grant -> X !grant) & G (cancel -> X (!grant U go))",  &["go", "cancel", "req"], &["grant"], Unrealizable),
    lilydemo11: ("!(G (req -> F ack) & G (go -> F grant))",  &["go", "req"], &["ack", "grant"], Unrealizable),
    lilydemo15: ("G (r1 -> F a1) & G (r2 -> F a2) & G !(a1 & a2) & (a1 W r1) & (a2 W r2)",  &["r1", "r2"], &["a1", "a2"], Unrealizable),
    lilydemo16: ("G (r0 -> F a0) & G (r1 -> F a1) & G (r2 -> F a2) & G !(a0 & a1) & G !(a0 & a2) & G !(a1 & a2) & (a0 W r0) & (a1 W r1) & (a2 W r2)",  &["r0", "r1", "r2"], &["a0", "a1", "a2"], Unrealizable),
    unused_unreal: ("(a | !b | c | X !c) && (e | !f | g | X !g)",  &["a", "b", "c", "d"], &["e", "f", "g", "h"], Unrealizable),
    biconditional_unreal: ("(F G a) <-> (G F b)", &["a"], &["b"], Unrealizable),
}

option_tests! {
    simple_real: ("(a & X !a)",  &[], &["a"], Realizable),
    simple_unreal: ("(a | X !a)",  &["a"], &[], Unrealizable),
    full_arbiter: ("(r0 R !g0) & (r1 R !g1) & G (!g0 | !g1) & G ((g1 & G !r1) -> (F !g0)) & G ((g1 & G !r1) -> (F !g1)) & G (r0 -> F g0) & G (r1 -> F g1) & G((g0 & X (!r0 & !g0)) -> X (r0 R !g0)) & G ((g1 & X (!r1 & !g1)) -> X (r1 R !g1))",
        &["r0", "r1"], &["g0", "g1"], Realizable),
    full_arbiter_unreal: ("(r0 R !g0) & (r1 R !g1) & G (!g0 | !g1) & G ((g1 & G !r1) -> (F !g0)) & G ((g1 & G !r1) -> (F !g1)) & G (r0 -> F g0) & G (r1 -> F g1) & G((g0 & X (!r0 & !g0)) -> X (r0 R !g0)) & G ((g1 & X (!r1 & !g1)) -> X (r1 R !g1)) & G ((r0 & X r1) -> X X (g0 & g1))",
        &["r0", "r1"], &["g0", "g1"], Unrealizable),
}
