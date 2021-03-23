use std::ffi::OsStr;
use std::fs;

use build_util::*;

fn main() {
    run_build_or_exit(build, "CUDD");
}

fn build() -> Result<(), BuildError> {
    let build_env = fetch_env()?;
    let cudd_dir = build_env.root_dir.join("c");

    // locate source files
    let directories = ["cudd", "mtr", "st", "util"];
    let mut c_files = Vec::new();
    let mut include_dirs = Vec::new();
    for dir in &directories {
        let dir_path = cudd_dir.join(dir);
        for entry in fs::read_dir(&dir_path)? {
            let entry = entry?;
            let path = entry.path();
            if path.extension() == Some(OsStr::new("c")) {
                c_files.push(path);
            }
        }
        include_dirs.push(dir_path);
    }

    // build cudd
    let mut build = cc::Build::new();
    build.warnings(false);
    build.flag_if_supported("-Wno-pointer-to-int-cast");

    // set config defines
    if build_env.profile == Profile::Debug {
        build.define("DD_DEBUG", None);
    }
    build.define("PACKAGE_VERSION", "\"3.0.0\"");
    build.define(
        "SIZEOF_INT",
        std::mem::size_of::<std::os::raw::c_int>()
            .to_string()
            .as_str(),
    );
    build.define(
        "SIZEOF_LONG",
        std::mem::size_of::<std::os::raw::c_long>()
            .to_string()
            .as_str(),
    );
    build.define(
        "SIZEOF_LONG_DOUBLE",
        std::mem::size_of::<std::os::raw::c_double>()
            .to_string()
            .as_str(),
    );
    build.define(
        "SIZEOF_VOID_P",
        std::mem::size_of::<*mut std::os::raw::c_void>()
            .to_string()
            .as_str(),
    );

    for dir in include_dirs {
        build.include(&dir);
    }
    for c_file in c_files {
        build.file(&c_file);
    }
    build.try_compile("cudd")?;

    // generate bindings to cudd headers
    let cudd_header = cudd_dir.join("cudd").join("cudd.h");
    bindgen::Builder::default()
        .header(format!("{}", cudd_header.display()))
        .generate()
        .map_err(|()| BuildError::Bindgen)?
        .write_to_file(build_env.out_dir.join("cudd_bindings.rs"))?;

    // link to cudd
    println!("cargo:rustc-link-lib=static=cudd");

    Ok(())
}
