use fs::File;
use fs_err as fs;
use std::env;
use std::fmt;
use std::io::{self, BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process;

#[derive(Debug)]
pub enum BuildError {
    IOError(io::Error),
    CommandExecutionError(String, io::Error),
    CommandStatusError(String, process::ExitStatus),
    EnvVarError(&'static str, env::VarError),
    CompilationError(cc::Error),
    UnknownProfileError(String),
    BindgenError,
}

impl fmt::Display for BuildError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            BuildError::IOError(e) => write!(f, "I/O error while building: {}", e),
            BuildError::CommandExecutionError(cmd, e) => write!(
                f,
                "The following command could not be executed: {}\n{}",
                e, cmd
            ),
            BuildError::CommandStatusError(cmd, e) => {
                write!(f, "The following command exited with {}\n{}", e, cmd)
            }
            BuildError::EnvVarError(var, e) => {
                write!(f, "Missing environment variable {}: {}", var, e)
            }
            BuildError::CompilationError(e) => write!(f, "Error during compilation: {}", e),
            BuildError::UnknownProfileError(p) => write!(f, "Unknown build profile: {}", p),
            BuildError::BindgenError => write!(f, "Error while generating bindings"),
        }
    }
}

impl From<io::Error> for BuildError {
    fn from(e: io::Error) -> Self {
        BuildError::IOError(e)
    }
}

impl From<cc::Error> for BuildError {
    fn from(e: cc::Error) -> Self {
        BuildError::CompilationError(e)
    }
}

pub fn run_build_or_exit<F>(build: F, name: &str)
where
    F: Fn() -> Result<(), BuildError>,
{
    let result = build();
    if let Err(e) = result {
        eprintln!("Error building {}: {}", name, e);
        process::exit(1)
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Profile {
    Debug,
    Release,
}

pub struct BuildEnv {
    pub out_dir: PathBuf,
    pub root_dir: PathBuf,
    pub profile: Profile,
}

fn get_profile(profile_str: String) -> Result<Profile, BuildError> {
    match profile_str.as_str() {
        "debug" => Ok(Profile::Debug),
        "release" => Ok(Profile::Release),
        _ => Err(BuildError::UnknownProfileError(profile_str)),
    }
}

fn env_var(var: &'static str) -> Result<String, BuildError> {
    env::var(var).map_err(|e| BuildError::EnvVarError(var, e))
}

pub fn fetch_env() -> Result<BuildEnv, BuildError> {
    let out_dir = PathBuf::from(&env_var("OUT_DIR")?);
    let root_dir = PathBuf::from(&env_var("CARGO_MANIFEST_DIR")?);
    let profile_str = env_var("PROFILE")?;
    let profile = get_profile(profile_str)?;
    Ok(BuildEnv {
        out_dir,
        root_dir,
        profile,
    })
}

pub fn run_command(mut command: process::Command) -> Result<(), BuildError> {
    let output = command
        .output()
        .map_err(|e| BuildError::CommandExecutionError(format!("{:?}", command), e))?;
    io::stderr().write_all(&output.stdout)?;
    io::stderr().write_all(&output.stderr)?;
    if output.status.success() {
        Ok(())
    } else {
        Err(BuildError::CommandStatusError(
            format!("{:?}", command),
            output.status,
        ))
    }
}

pub fn patch_file(
    source: &Path,
    destination: &Path,
    pattern: &str,
    replacements: &[&str],
) -> Result<(), io::Error> {
    let file_in = File::open(source)?;
    let mut file_out = File::create(destination)?;
    let reader = BufReader::new(file_in);
    for line in reader.lines() {
        let line = line?;
        if line == pattern {
            for replacement in replacements {
                writeln!(file_out, "{}", replacement)?;
            }
        } else {
            writeln!(file_out, "{}", line)?;
        }
    }
    Ok(())
}
