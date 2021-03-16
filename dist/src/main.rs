use std::env::{self, consts};
use std::ffi::OsStr;
use std::fmt::{self, Debug, Display};
use std::fs::{self, File};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process::{self, Command};
use std::str::FromStr;

use regex::Regex;
use sha2::{Digest, Sha256};
use walkdir::WalkDir;

const PACKAGE_NAME: &str = "strix";
const BIN_NAME: &str = PACKAGE_NAME;
const LIB_NAME: &str = "owl";

#[derive(Copy, Clone, Debug)]
enum PackageType {
    None,
    Tar,
    Pkg,
    Deb,
}

type DynError = Box<dyn std::error::Error>;

#[derive(Debug)]
struct DisplayError<T> {
    msg: T,
    source: Option<Box<(dyn std::error::Error + 'static)>>,
}

impl<T: Debug + Display> DisplayError<T> {
    fn new(msg: T) -> Box<Self> {
        Box::new(DisplayError { msg, source: None })
    }

    fn with_source(msg: T, source: DynError) -> Box<Self> {
        Box::new(DisplayError {
            msg,
            source: Some(source),
        })
    }
}

impl<T: Display> Display for DisplayError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.msg)
    }
}

impl<T: Debug + Display> std::error::Error for DisplayError<T> {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        self.source.as_ref().map(|e| e.as_ref())
    }
}

fn main() {
    if let Err(error) = try_main() {
        let mut error = error.as_ref();
        eprintln!("Error: {}", error);
        while let Some(source) = error.source() {
            eprintln!("Cause: {}", source);
            error = source;
        }
        process::exit(1);
    }
}

fn try_main() -> Result<(), DynError> {
    let task = env::args().nth(1);
    match task.as_deref() {
        Some("build") => dist(PackageType::None)?,
        Some("build-tar") => dist(PackageType::Tar)?,
        Some("build-pkg") => dist(PackageType::Pkg)?,
        Some("build-deb") => dist(PackageType::Deb)?,
        _ => print_help(),
    }
    Ok(())
}

fn print_help() {
    eprintln!(
        "Tasks:
  build           build binary files for generic binary distribution
  build-tar       build and archive binary files for generic binary distribution
  build-pkg       builds binary distribution for Arch Linux/Manjaro systems
  build-deb       builds binary distribution for Debian/Ubuntu systems
"
    )
}

fn dist(pt: PackageType) -> Result<(), DynError> {
    println!("Obtaining crate metadata...");

    let arch = match pt {
        PackageType::Pkg => match consts::ARCH {
            "x86" => "i686",
            "x86_64" => "x86_64",
            _ => {
                return Err(DisplayError::new(format!(
                    "unsupported architecture for pkg distribution: {}",
                    consts::ARCH
                )))
            }
        },
        PackageType::Deb => match consts::ARCH {
            "x86" => "i386",
            "x86_64" => "amd64",
            _ => {
                return Err(DisplayError::new(format!(
                    "unsupported architecture for deb distribution: {}",
                    consts::ARCH
                )))
            }
        },
        PackageType::None | PackageType::Tar => consts::ARCH,
    };

    let mut cmd = cargo_metadata::MetadataCommand::new();
    cmd.no_deps();
    let metadata = cmd.exec().map_err(|err| {
        DisplayError::with_source("Could not obtain crate metadata", Box::new(err))
    })?;

    let root_dir = metadata.workspace_root;
    let target_dir = metadata.target_directory;
    let out_dir = target_dir.join("release");

    let package = metadata
        .packages
        .iter()
        .find(|p| p.name == PACKAGE_NAME)
        .ok_or_else(|| DisplayError::new(format!("package {} not found", PACKAGE_NAME)))?;

    let version = format!("{}", package.version);

    let author = package.authors.get(0).map(std::ops::Deref::deref);
    let repository = package.repository.as_deref();

    let description = package.description.as_deref();
    let license = package.license.as_deref();

    println!("Building package...");
    run_build(&root_dir)
        .map_err(|err| DisplayError::with_source("Could not build package", err))?;

    let dist_dir = target_dir.join("dist");
    println!("Clearing dist directory...");
    clear_dir(&dist_dir)
        .map_err(|err| DisplayError::with_source("Could not clear dist directory", err))?;

    println!("Copying binary and library files...");
    let bin_str = format!("{}{}", BIN_NAME, consts::EXE_SUFFIX);
    let bin = out_dir.join(&bin_str);

    let lib_str = format!("{}{}{}", consts::DLL_PREFIX, LIB_NAME, consts::DLL_SUFFIX);
    let lib_os_str = OsStr::new(&lib_str);

    let lib = find_newest(&out_dir, &lib_os_str)
        .map_err(|err| DisplayError::with_source("Could not find Owl library", err))?;

    let base = PackageBase {
        name: PACKAGE_NAME,
        ver: &version,
        rel: 1,
        arch: &arch,
    };

    let package_dirs = copy(pt, &base, &dist_dir, &bin, &lib, &bin_str, &lib_str)
        .map_err(|err| DisplayError::with_source("Could not copy files for package: {}", err))?;

    println!("Computing hashsums...");
    let bin_hash = get_hash(&bin).map_err(|err| {
        DisplayError::with_source(format!("Could not compute {} binary hash", BIN_NAME), err)
    })?;
    let lib_hash = get_hash(&lib).map_err(|err| {
        DisplayError::with_source(format!("Could not compute {} library hash", LIB_NAME), err)
    })?;

    println!("Querying versions of dependenies...");
    let dependencies = get_dependencies(&bin, &lib)?;

    println!("Creating package information...");
    let package_info = PackageInfo {
        base,
        author,
        desc: description,
        license,
        repository,
        bin_file: &bin_str,
        lib_file: &lib_str,
        bin_sha256sum: &bin_hash,
        lib_sha256sum: &lib_hash,
        dependencies,
    };

    match pt {
        PackageType::Pkg => write_pkgbuild(&package_info, &package_dirs.package_dir)
            .map_err(|err| DisplayError::with_source("Could not create PKGBUILD", err))?,
        PackageType::Deb => write_debbuild(&package_info, &package_dirs.package_dir)
            .map_err(|err| DisplayError::with_source("Could not create DEBIAN config", err))?,
        PackageType::None | PackageType::Tar => (),
    };

    println!("Building package...");
    match pt {
        PackageType::Pkg => run_makepkg(&package_info, &package_dirs.package_dir)
            .map_err(|err| DisplayError::with_source("Could not run makepkg", err)),
        PackageType::Deb => run_dpkgdeb(&package_info, &package_dirs.package_dir)
            .map_err(|err| DisplayError::with_source("Could not run dpkg-deb", err)),
        PackageType::Tar => run_tar(&package_info, &package_dirs.package_dir)
            .map_err(|err| DisplayError::with_source("Could not run tar", err)),
        PackageType::None => Ok(()),
    }
    .map_err(|err| DisplayError::with_source("Could not build package", err))?;

    println!(
        "Success: distribution for {} available in {}",
        package_info.base,
        dist_dir.display()
    );
    Ok(())
}

#[derive(Debug)]
struct PackageBase<'a> {
    name: &'a str,
    ver: &'a str,
    rel: u32,
    arch: &'a str,
}

impl Display for PackageBase<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}-{}-{}-{}", self.name, self.ver, self.rel, self.arch)
    }
}

#[derive(Debug)]
struct PackageInfo<'a> {
    base: PackageBase<'a>,
    author: Option<&'a str>,
    desc: Option<&'a str>,
    license: Option<&'a str>,
    repository: Option<&'a str>,
    bin_file: &'a str,
    lib_file: &'a str,
    bin_sha256sum: &'a str,
    lib_sha256sum: &'a str,
    dependencies: Dependencies,
}

#[derive(Debug)]
struct PackageDirStructure {
    bin_target: PathBuf,
    lib_target: PathBuf,
    package_dir: PathBuf,
}

fn clear_dir<P: AsRef<Path>>(path: P) -> Result<(), DynError> {
    let path = path.as_ref();
    if path.exists() {
        let file_type = fs::metadata(path)?.file_type();
        if file_type.is_dir() {
            fs::remove_dir_all(path)?;
        } else {
            fs::remove_file(path)?;
        }
    }
    Ok(())
}

fn copy<P: AsRef<Path>>(
    pt: PackageType,
    pkg: &PackageBase,
    dist_dir: P,
    bin: P,
    lib: P,
    bin_str: &str,
    lib_str: &str,
) -> Result<PackageDirStructure, DynError> {
    let dist_dir = dist_dir.as_ref();
    fs::create_dir_all(&dist_dir)?;

    let structure = match pt {
        PackageType::Pkg | PackageType::None | PackageType::Tar => {
            let bin_target = dist_dir.join(&bin_str);
            let lib_target = dist_dir.join(&lib_str);
            let package_dir = dist_dir.to_path_buf();
            PackageDirStructure {
                bin_target,
                lib_target,
                package_dir,
            }
        }
        PackageType::Deb => {
            let package_name = format!("{}-{}-{}-{}", pkg.name, pkg.ver, pkg.rel, pkg.arch);
            let package_dir = dist_dir.join(package_name);
            let usr_dir = package_dir.join("usr");
            let usr_bin_dir = usr_dir.join("bin");
            let usr_lib_dir = usr_dir.join("lib");
            fs::create_dir_all(&usr_bin_dir)?;
            fs::create_dir_all(&usr_lib_dir)?;
            let bin_target = usr_bin_dir.join(&bin_str);
            let lib_target = usr_lib_dir.join(&lib_str);
            PackageDirStructure {
                bin_target,
                lib_target,
                package_dir,
            }
        }
    };

    fs::copy(&bin, &structure.bin_target)?;
    fs::copy(&lib, &structure.lib_target)?;

    Ok(structure)
}

fn run_build<P: AsRef<Path>>(path: P) -> Result<(), DynError> {
    let cargo = env::var("CARGO").unwrap_or_else(|_| "cargo".to_string());
    let result = Command::new(cargo)
        .current_dir(path)
        .args(&["build", "--release"])
        .status()?;

    if result.success() {
        Ok(())
    } else {
        Err(DisplayError::new(format!(
            "cargo build failed with exit code {}",
            result
        )))
    }
}

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
struct DepVersion {
    version: Vec<u32>,
}

impl Display for DepVersion {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for (i, v) in self.version.iter().enumerate() {
            if i > 0 {
                write!(f, ".")?;
            }
            write!(f, "{}", v)?;
        }
        Ok(())
    }
}

impl DepVersion {
    fn new(version: &[u32]) -> Self {
        DepVersion {
            version: version.to_vec(),
        }
    }
}

impl FromStr for DepVersion {
    type Err = <u32 as FromStr>::Err;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(DepVersion {
            version: s
                .split('.')
                .map(|v| v.parse::<u32>())
                .collect::<Result<Vec<_>, _>>()?,
        })
    }
}

#[derive(Debug)]
struct Dependencies {
    glibc: DepVersion,
    gcc_libs: DepVersion,
    zlib: DepVersion,
}

fn max_version(text: &str, prefix: &str) -> Option<DepVersion> {
    let re = Regex::new(&format!("{}_([0-9.]+)", prefix)).unwrap();
    re.captures_iter(text)
        .map(|c| c.get(1).unwrap().as_str().parse::<DepVersion>().unwrap())
        .max()
}

fn get_dependencies<P: AsRef<Path>>(bin: P, lib: P) -> Result<Dependencies, DynError> {
    let result = Command::new("readelf")
        .arg("-V")
        .arg(lib.as_ref())
        .arg(bin.as_ref())
        .output()?;
    let status = result.status;
    let output = String::from_utf8_lossy(&result.stdout);
    if status.success() {
        let glibc = max_version(&output, "GLIBC")
            .ok_or_else(|| DisplayError::new("error: unexpectly found no glibc dependency"))?;
        let gcc_libs = max_version(&output, "GCC")
            .ok_or_else(|| DisplayError::new("error: unexpectly found no gcc dependency"))?;
        // GraalVM native-image adds a dependency to zlib, but does use any ZLIB symbols.
        // Therefore we add a sensible default version.
        let zlib = max_version(&output, "ZLIB").unwrap_or_else(|| DepVersion::new(&[1, 2, 7]));
        Ok(Dependencies {
            glibc,
            gcc_libs,
            zlib,
        })
    } else {
        Err(DisplayError::new(format!(
            "readelf failed with exit code {} and output:\n{}\n{}",
            status,
            output,
            String::from_utf8_lossy(&result.stderr),
        )))
    }
}

fn run_tar<P: AsRef<Path>>(pkg: &PackageInfo, path: P) -> Result<(), DynError> {
    let mut cmd = Command::new("tar");
    cmd.current_dir(path);
    cmd.args(&["-c", "-z", "-f"]);
    cmd.arg(format!("{}.tar.gz", pkg.base));
    cmd.args(&[pkg.bin_file, pkg.lib_file]);
    let result = cmd.status()?;
    if result.success() {
        Ok(())
    } else {
        Err(DisplayError::new(format!(
            "tar failed with exit code {}",
            result
        )))
    }
}

fn run_makepkg<P: AsRef<Path>>(pkg: &PackageInfo, path: P) -> Result<(), DynError> {
    let mut cmd = Command::new("makepkg");
    cmd.current_dir(path);
    if let Some(author) = pkg.author {
        cmd.env("PACKAGER", author);
    }
    let result = cmd.status()?;
    if result.success() {
        Ok(())
    } else {
        Err(DisplayError::new(format!(
            "makepkg failed with exit code {}",
            result
        )))
    }
}

fn run_dpkgdeb<P: AsRef<Path>>(_: &PackageInfo, path: P) -> Result<(), DynError> {
    let result = Command::new("dpkg-deb")
        .arg("--build")
        .arg(format!("{}", path.as_ref().display()))
        .status()?;
    if result.success() {
        Ok(())
    } else {
        Err(DisplayError::new(format!(
            "dpkg-deb failed with exit code {}",
            result
        )))
    }
}

fn write_pkgbuild<P: AsRef<Path>>(pkg: &PackageInfo, path: P) -> Result<(), DynError> {
    let pkgbuild_path = path.as_ref().join("PKGBUILD");
    let mut file = File::create(pkgbuild_path)?;
    writeln!(file, "pkgname={}", pkg.base.name)?;
    writeln!(file, "pkgver={}", pkg.base.ver)?;
    writeln!(file, "pkgrel={}", pkg.base.rel)?;
    if let Some(desc) = pkg.desc {
        writeln!(file, "pkgdesc='{}'", desc)?;
    }
    writeln!(file, "arch=('{}')", pkg.base.arch)?;
    if let Some(license) = pkg.license {
        writeln!(file, "license=('{}')", license)?;
    }
    if let Some(repository) = pkg.repository {
        writeln!(file, "url='{}'", repository)?;
    }

    writeln!(file, "depends=(")?;
    writeln!(file, "  'glibc>={}'", pkg.dependencies.glibc)?;
    writeln!(file, "  'gcc-libs>={}'", pkg.dependencies.gcc_libs)?;
    writeln!(file, "  'zlib>={}'", pkg.dependencies.zlib)?;
    writeln!(file, ")")?;

    writeln!(file, "source=(")?;
    writeln!(file, "  '{}'", pkg.bin_file)?;
    writeln!(file, "  '{}'", pkg.lib_file)?;
    writeln!(file, ")")?;

    writeln!(file, "sha256sums=(")?;
    writeln!(file, "  '{}'", pkg.bin_sha256sum)?;
    writeln!(file, "  '{}'", pkg.lib_sha256sum)?;
    writeln!(file, ")")?;

    writeln!(file, "package() {{")?;
    writeln!(file, "  mkdir -p $pkgdir/usr/bin")?;
    writeln!(file, "  mkdir -p $pkgdir/usr/lib")?;
    writeln!(file, "  cp '{}' $pkgdir/usr/bin/", pkg.bin_file)?;
    writeln!(file, "  cp '{}' $pkgdir/usr/lib/", pkg.lib_file)?;
    writeln!(file, "}}")?;

    Ok(())
}

fn write_debbuild<P: AsRef<Path>>(pkg: &PackageInfo, path: P) -> Result<(), DynError> {
    let config_dir = path.as_ref().join("DEBIAN");
    fs::create_dir_all(&config_dir)?;
    let control_path = config_dir.join("control");
    let sha256sums_path = config_dir.join("sha256sums");

    let mut file = File::create(control_path)?;
    writeln!(file, "Package: {}", pkg.base.name)?;
    writeln!(file, "Version: {}", pkg.base.ver)?;
    writeln!(file, "Architecture: {}", pkg.base.arch)?;
    writeln!(file, "Priority: optional")?;
    writeln!(
        file,
        "Depends: glibc (>= {}), libgcc1 (>= {}), zlib1g (>= {})",
        pkg.dependencies.glibc, pkg.dependencies.gcc_libs, pkg.dependencies.zlib
    )?;
    if let Some(author) = pkg.author {
        writeln!(file, "Maintainer: {}", author)?;
    }
    if let Some(desc) = pkg.desc {
        writeln!(file, "Description: {}", desc)?;
    }

    let mut file = File::create(sha256sums_path)?;
    writeln!(file, "{} {}", pkg.bin_sha256sum, pkg.bin_file)?;
    writeln!(file, "{} {}", pkg.lib_sha256sum, pkg.lib_file)?;
    Ok(())
}

fn get_hash<P: AsRef<Path>>(file: P) -> Result<String, DynError> {
    let mut file = File::open(file)?;
    let mut hasher = Sha256::new();
    io::copy(&mut file, &mut hasher)?;
    let result = hasher.finalize();
    Ok(format!("{:x}", result))
}

fn find_newest<P: AsRef<Path>>(path: P, name: &OsStr) -> Result<PathBuf, DynError> {
    let mut lib = PathBuf::new();
    let mut most_recent = std::time::SystemTime::UNIX_EPOCH;
    for entry in WalkDir::new(path) {
        let entry = entry?;
        let path = entry.path();
        if path.file_name() == Some(name) {
            let file_metadata = fs::metadata(&path)?;
            let last_modified = file_metadata.modified()?;

            if most_recent < last_modified {
                lib.clear();
                lib.push(path);
                most_recent = last_modified;
            }
        }
    }
    Ok(lib)
}
