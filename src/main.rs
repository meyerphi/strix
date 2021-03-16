use std::io;

use clap::Clap;
use fs_err as fs;

use strix::options::{Options, OutputFormat, SynthesisOptions, TraceLevel};
use strix::synthesize_with;

fn main() {
    if let Err(error) = strix_main() {
        eprintln!("Error: {}", error);
        std::process::exit(1);
    }
}

fn set_up_logging(level: TraceLevel) -> io::Result<()> {
    env_logger::builder()
        .filter(None, level.into())
        .format_timestamp_millis()
        .try_init()
        .map_err(|e| io::Error::new(io::ErrorKind::AlreadyExists, e))
}

fn strix_main() -> io::Result<()> {
    let options = Options::parse();
    set_up_logging(options.trace_level)?;

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
        && !matches!(options.output_format, OutputFormat::AAG | OutputFormat::AIG)
    {
        synthesis_options.output_format = OutputFormat::AAG;
    }
    let result = synthesize_with(&ltl, &ins, &outs, &synthesis_options);

    println!("{}", result.status);
    if let Some(controller) = result.controller {
        let binary = options.output_format == OutputFormat::AIG;
        if let Some(output_file) = &options.output_file {
            let file = fs::File::create(output_file)?;
            controller.write(file, binary)?;
        } else {
            controller.write(std::io::stdout(), binary)?;
        }
    }
    Ok(())
}
