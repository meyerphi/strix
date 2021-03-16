use std::io::Write;
use std::path::PathBuf;
use std::process::Command;

use strix::options::*;
use strix::{synthesize_with, Controller, Status, Status::*};

fn verify_realizability(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    verify_realizability_with(
        ltl,
        ins,
        outs,
        expected_status,
        SynthesisOptions {
            only_realizability: true,
            ..Default::default()
        },
    );
}

fn verify_realizability_with(
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    expected_status: Status,
    options: SynthesisOptions,
) {
    let result = synthesize_with(ltl, ins, outs, &options);
    assert_eq!(result.status, expected_status);
}

fn verify_implementation<T: std::fmt::Display>(
    machine: T,
    script_file: &str,
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    status: Status,
) {
    let mut implementation_file = tempfile::NamedTempFile::new().unwrap();
    write!(implementation_file, "{}", machine).unwrap();

    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let script = root.join("scripts").join(script_file);

    let verification_status = Command::new(script)
        .arg(implementation_file.path())
        .arg(ltl)
        .arg(ins.join(","))
        .arg(outs.join(","))
        .arg(format!("{}", status))
        .status()
        .expect("failed to execute verification script");
    assert!(verification_status.success());
}

fn verify_aiger(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    verify_aiger_with(
        ltl,
        ins,
        outs,
        expected_status,
        SynthesisOptions {
            output_format: OutputFormat::AAG,
            aiger_portfolio: true,
            ..Default::default()
        },
    );
}

fn verify_aiger_with(
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    expected_status: Status,
    options: SynthesisOptions,
) {
    let result = synthesize_with(ltl, ins, outs, &options);
    assert_eq!(result.status, expected_status);
    if let Some(Controller::Aiger(aiger)) = result.controller {
        verify_implementation(aiger, "verify_aiger.sh", ltl, ins, outs, expected_status);
    } else {
        panic!("no aiger controller produced");
    }
}

fn verify_hoa(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    verify_hoa_with(
        ltl,
        ins,
        outs,
        expected_status,
        SynthesisOptions {
            output_format: OutputFormat::HOA,
            ..Default::default()
        },
    );
}

fn verify_hoa_with(
    ltl: &str,
    ins: &[&str],
    outs: &[&str],
    expected_status: Status,
    options: SynthesisOptions,
) {
    let result = synthesize_with(ltl, ins, outs, &options);
    assert_eq!(result.status, expected_status);
    if let Some(Controller::Machine(machine)) = result.controller {
        verify_implementation(machine, "verify_hoa.sh", ltl, ins, outs, expected_status);
    } else {
        panic!("no machine controller produced");
    }
}

fn verify_pg(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    let options = SynthesisOptions {
        output_format: OutputFormat::PG,
        ..Default::default()
    };
    let result = synthesize_with(ltl, ins, outs, &options);
    assert_eq!(result.status, expected_status);
    // can not verify parity game itself currently
    assert!(matches!(result.controller, Some(Controller::ParityGame(_))));
}

fn verify_bdd(ltl: &str, ins: &[&str], outs: &[&str], expected_status: Status) {
    let options = SynthesisOptions {
        output_format: OutputFormat::BDD,
        ..Default::default()
    };
    let result = synthesize_with(ltl, ins, outs, &options);
    assert_eq!(result.status, expected_status);
    // can not verify BDD itself currently
    assert!(matches!(result.controller, Some(Controller::BDD(_))));
}

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

macro_rules! option_tests {
    ($($name:ident: ($ltl:expr, $ins:expr, $outs:expr, $expected_status:expr),)*) => {
        mod exploration_bfs {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_strategy: ExplorationStrategy::BFS,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod exploration_dfs {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_strategy: ExplorationStrategy::DFS,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod exploration_min {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_strategy: ExplorationStrategy::Min,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod exploration_max {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_strategy: ExplorationStrategy::Max,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod exploration_minmax {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_strategy: ExplorationStrategy::MinMax,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod onthefly_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_on_the_fly: OnTheFlyLimit::None,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod onthefly_node1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_on_the_fly: OnTheFlyLimit::Nodes(1),
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod onthefly_edge1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_on_the_fly: OnTheFlyLimit::Edges(1),
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod onthefly_state1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_on_the_fly: OnTheFlyLimit::States(1),
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod onthefly_seconds1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_on_the_fly: OnTheFlyLimit::Seconds(1),
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod onthefly_multiple1 {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_on_the_fly: OnTheFlyLimit::TimeMultiple(1),
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod solver_si {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        parity_solver: Solver::SI,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod solver_dfi {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        parity_solver: Solver::FPI,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
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
                        output_format: OutputFormat::AAG,
                        parity_solver: Solver::ZLK,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod simplification_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        ltl_simplification: Simplification::None,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod simplification_language {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        ltl_simplification: Simplification::Language,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod simplification_realizability {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        ltl_simplification: Simplification::Realizability,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod label_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        label_structure: LabelStructure::None,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod label_outer {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        label_structure: LabelStructure::Outer,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod label_inner {
            use super::*;
            $(
                #[test]
                #[ignore] // Not yet supported
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        label_structure: LabelStructure::Inner,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod reordering_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        bdd_reordering: BddReordering::None,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod reordering_heuristic {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        bdd_reordering: BddReordering::Heuristic,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod reordering_mixed {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        bdd_reordering: BddReordering::Mixed,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod reordering_exact {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        bdd_reordering: BddReordering::Exact,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod compression_none {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        aiger_compression: AigerCompression::None,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod compression_basic {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        aiger_compression: AigerCompression::Basic,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod compression_more {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        aiger_compression: AigerCompression::More,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
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
                        output_format: OutputFormat::AIG,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod determinization_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::HOA,
                        machine_determinization: true,
                        ..Default::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_none_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::HOA,
                        machine_minimization: MinimizationMethod::None,
                        ..Default::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_nondet_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::HOA,
                        machine_minimization: MinimizationMethod::NonDeterminism,
                        ..Default::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_dontcares_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::HOA,
                        machine_minimization: MinimizationMethod::DontCares,
                        ..Default::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_both_hoa {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::HOA,
                        machine_minimization: MinimizationMethod::Both,
                        ..Default::default()
                    };
                    verify_hoa_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_none_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        machine_minimization: MinimizationMethod::None,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_nondet_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        machine_minimization: MinimizationMethod::NonDeterminism,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_dontcares_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        machine_minimization: MinimizationMethod::DontCares,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod minimization_both_aag {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        machine_minimization: MinimizationMethod::Both,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod aiger_portfolio {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        aiger_portfolio: true,
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
                }
            )*
        }
        mod exploration_filter {
            use super::*;
            $(
                #[test]
                fn $name() {
                    let options = SynthesisOptions {
                        output_format: OutputFormat::AAG,
                        exploration_filter: true,
                        exploration_on_the_fly: OnTheFlyLimit::Nodes(1),
                        ..Default::default()
                    };
                    verify_aiger_with($ltl, $ins, $outs, $expected_status, options);
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
    ltl2dba11: ("(G (p U (q U ((!p) U !q)))) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba12: ("(((G p) -> F q) & ((G !p) -> F !q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba13: ("((G !r) & (G (p -> F q)) & (G (q -> F r))) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba14: ("((G (p -> F q)) & (G r)) <-> G F acc",  &["p", "q", "r"], &["acc"] , Realizable),
    ltl2dba15: ("(G F (p -> X X X q)) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
    ltl2dba16: ("((G (p -> F q)) & (G ((!p) -> F !q))) <-> G F acc",  &["p", "q"], &["acc"] , Realizable),
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
    unused_real: ("(a & !b & c & X !c) | (e & !f & g & X !g)",  &["a", "b", "c", "d"], &["e", "f", "g", "h"], Realizable),
    xg: ("a & XG!a", &[], &["a"], Realizable),
    xgf: ("X G (a -> F b) <-> G F acc", &["a", "b"], &["acc"], Realizable),

    ltl_false: ("false",  &[], &[], Unrealizable),
    ltl2dba27: ("(F G !p) <-> G F acc",  &["p"], &["acc"], Unrealizable),
    lilydemo01: ("G (req -> X (grant & X (grant & X grant))) & G (grant -> X !grant) & G (cancel -> X ((!grant) U go))",  &["go", "cancel", "req"], &["grant"], Unrealizable),
    lilydemo02: ("G (req -> X (grant | X (grant | X grant))) & G (grant -> X !grant) & G (cancel -> X ((!grant) U go))",  &["go", "cancel", "req"], &["grant"], Unrealizable),
    lilydemo11: ("! (G (req -> F ack) & G (go -> F grant))",  &["go", "req"], &["ack", "grant"], Unrealizable),
    lilydemo15: ("G (r1 -> F a1) & G (r2 -> F a2) & G (! (a1 & a2)) & (a1 W r1) & (a2 W r2)",  &["r1", "r2"], &["a1", "a2"], Unrealizable),
    lilydemo16: ("G (r0 -> F a0) & G (r1 -> F a1) & G (r2 -> F a2) & G (! (a0 & a1)) & G (! (a0 & a2)) & G (! (a1 & a2)) & (a0 W r0) & (a1 W r1) & (a2 W r2)",  &["r0", "r1", "r2"], &["a0", "a1", "a2"], Unrealizable),
    ltl2dba_theta: ("!((G F p0) & G (q -> F r)) <-> G F acc", &["r", "q", "p0"], &["acc"], Unrealizable),
    unused_unreal: ("(a || !b || c || X !c) && (e || !f || g || X !g)",  &["a", "b", "c", "d"], &["e", "f", "g", "h"], Unrealizable),
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
