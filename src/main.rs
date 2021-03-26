//! Strix binary crate.

use std::io::{self, Write};

use clap::Clap;
use fs_err as fs;

use strix::options::{CliOptions, OutputFormat, SynthesisOptions, TraceLevel};
use strix::synthesize_with;

fn main() {
    if let Err(error) = strix_main() {
        // discard result as we cannot further propagate a write error
        let _ = write!(io::stderr(), "Error: {}", error);
        std::process::exit(1);
    }
}

/// Initialize the logging framework with the given trace level.
///
/// # Errors
///
/// Returns an error if the logging framework has already been initialized.
fn initialize_logging(level: TraceLevel) -> io::Result<()> {
    env_logger::builder()
        .filter(None, level.into())
        .format_timestamp_millis()
        .try_init()
        .map_err(|e| io::Error::new(io::ErrorKind::AlreadyExists, e))
}

/// Main function that parses the options, reads the input,
/// calls the synthesis procedure and writes the output.
///
/// # Errors
///
/// Returns an error if an I/O error occurred, e.g. from opening a file.
fn strix_main() -> io::Result<()> {
    let options = CliOptions::parse();
    initialize_logging(options.trace_level)?;

    // trim inputs and outputs
    let ins: Vec<_> = options.inputs.iter().map(|s| s.trim()).collect();
    let outs: Vec<_> = options.outputs.iter().map(|s| s.trim()).collect();

    let ltl = if let Some(input_file) = &options.input_file {
        fs::read_to_string(input_file)?
    } else if let Some(formula) = &options.formula {
        formula.clone()
    } else {
        unreachable!()
    };

    let mut synthesis_options = SynthesisOptions::from(&options);
    // override output option for aiger portfolio option
    if options.aiger_portfolio
        && !matches!(options.output_format, OutputFormat::Aag | OutputFormat::Aig)
    {
        synthesis_options.output_format = OutputFormat::Aag;
    }
    let result = synthesize_with(&ltl, &ins, &outs, &synthesis_options);

    writeln!(io::stdout(), "{}", result.status())?;
    if let Some(controller) = result.controller() {
        let binary = synthesis_options.output_format == OutputFormat::Aig;
        if let Some(output_file) = &options.output_file {
            let file = fs::File::create(output_file)?;
            controller.write(file, result.status(), binary)?;
        } else {
            controller.write(io::stdout(), result.status(), binary)?;
        }
    }
    Ok(())
}
