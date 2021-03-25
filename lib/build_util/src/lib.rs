//! Utility functions for build process with compilation of external code.

use fs::File;
use fs_err as fs;
use std::env;
use std::error::Error;
use std::fmt;
use std::io::{self, BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process;

/// An error during the build process.
#[derive(Debug)]
pub enum BuildError {
    /// An error from an underlying I/O operation.
    Io(io::Error),
    /// An I/O error during execution of a command, containing the command string
    /// and the underlying I/O error.
    CommandExecution(String, io::Error),
    /// An error due to a process not exiting successfully, containing the
    /// command string and the exit status.
    CommandStatus(String, process::ExitStatus),
    /// An error due to a failed attempt to read an environment variable,
    /// containing the variable and the underlying error.
    EnvVar(&'static str, env::VarError),
    /// An error during compilation by the [cc] crate.
    Compilation(cc::Error),
    /// An error resulting from an unidentified build profile, containing the
    /// build profile.
    UnknownProfile(String),
    /// An error from the [bindgen] crate.
    Bindgen,
}

impl fmt::Display for BuildError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::Io(e) => write!(f, "I/O error while building: {}", e),
            Self::CommandExecution(cmd, e) => write!(
                f,
                "The following command could not be executed: {}\n{}",
                e, cmd
            ),
            Self::CommandStatus(cmd, e) => {
                write!(f, "The following command exited with {}\n{}", e, cmd)
            }
            Self::EnvVar(var, e) => {
                write!(f, "Missing environment variable {}: {}", var, e)
            }
            Self::Compilation(e) => write!(f, "Error during compilation: {}", e),
            Self::UnknownProfile(p) => write!(f, "Unknown build profile: {}", p),
            Self::Bindgen => write!(f, "Error while generating bindings"),
        }
    }
}

impl Error for BuildError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            BuildError::Io(err) => Some(err),
            BuildError::CommandExecution(_, err) => Some(err),
            BuildError::CommandStatus(_, _) => None,
            BuildError::EnvVar(_, err) => Some(err),
            BuildError::Compilation(err) => Some(err),
            BuildError::UnknownProfile(_) => None,
            BuildError::Bindgen => None,
        }
    }
}

impl From<io::Error> for BuildError {
    fn from(e: io::Error) -> Self {
        Self::Io(e)
    }
}

impl From<cc::Error> for BuildError {
    fn from(e: cc::Error) -> Self {
        Self::Compilation(e)
    }
}

/// Runs the given build function. On an error,
/// prints the returned error annotated with `name`
/// and exists the current process with status 1.
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

/// A build profile.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Profile {
    /// Debug profile.
    Debug,
    /// Release profile.
    Release,
}

/// Environment variables for the build.
pub struct BuildEnv {
    /// The output directory of cargo.
    pub out_dir: PathBuf,
    /// The root directory of the crate.
    pub root_dir: PathBuf,
    /// The build profile.
    pub profile: Profile,
}

/// Parses the build profile from the given profile string.
///
/// # Errors
///
/// Returns an error of type [`BuildError::UnknownProfile`]
/// if the profile can not be identified.
fn get_profile(profile_str: String) -> Result<Profile, BuildError> {
    match profile_str.as_str() {
        "debug" => Ok(Profile::Debug),
        "release" => Ok(Profile::Release),
        _ => Err(BuildError::UnknownProfile(profile_str)),
    }
}

/// Fetches the environment variable `var`.
///
/// # Errors
///
/// Returns an error of type [`BuildError::EnvVar`] if the environment variable
/// can not be fetched.
fn env_var(var: &'static str) -> Result<String, BuildError> {
    env::var(var).map_err(|e| BuildError::EnvVar(var, e))
}

/// Returns a build enviroment with the values of the environment variables for cargo.
///
/// # Errors
///
/// Returns an error of type [`BuildError::EnvVar`] if an environment variable is not found,
/// or an error of type [`BuildError::UnknownProfile`] if the build profile could not be determined.
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

/// Executes the given command.
///
/// Any output of the command (both to stdout and to stderr) is printed to stderr of the current process.
///
/// # Errors
///
/// Returns an error of type [`BuildError::CommandExecution`] if the command fails to execute, containing
/// the command, its arguments and the underlying error.
/// Returns an error of type [`BuildError::CommandStatus`] if the process does not exit successfully.
pub fn run_command(mut command: process::Command) -> Result<(), BuildError> {
    let output = command
        .output()
        .map_err(|e| BuildError::CommandExecution(format!("{:?}", command), e))?;
    io::stderr().write_all(&output.stdout)?;
    io::stderr().write_all(&output.stderr)?;
    if output.status.success() {
        Ok(())
    } else {
        Err(BuildError::CommandStatus(
            format!("{:?}", command),
            output.status,
        ))
    }
}

/// Replaces any line matching `pattern` in the file `source` with the lines in `replacements`,
/// and writes the patched file to `destination`.
///
/// # Errors
///
/// Returns an I/O error if reading from `source` or writing to `destination` fails.
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
