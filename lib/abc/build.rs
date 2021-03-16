use std::ffi::OsStr;
use walkdir::WalkDir;

use build_util::*;

fn main() {
    run_build_or_exit(build, "ABC");
}

fn build() -> Result<(), BuildError> {
    let build_env = fetch_env()?;
    let abc_dir = build_env.root_dir.join("c");

    let mut c_files: Vec<std::path::PathBuf> = Vec::new();
    // locate source files
    for entry in WalkDir::new(&abc_dir) {
        let entry = entry.map_err(std::io::Error::from)?;
        let path = entry.path();
        if path.extension() == Some(OsStr::new("c")) {
            c_files.push(path.to_path_buf());
        }
    }

    // build abc
    let mut build = cc::Build::new();
    build.flag_if_supported("-Wno-shift-negative-value");

    // set config defines
    let lin = if std::mem::size_of::<*mut std::os::raw::c_void>() == 8 {
        "LIN64"
    } else {
        "LIN"
    };
    build.define(lin, None);

    // add aiger header
    let aiger_dir = build_env.root_dir.parent().unwrap().join("aiger").join("c");

    build.include(&abc_dir);
    build.include(&aiger_dir);
    for c_file in c_files {
        build.file(&c_file);
    }
    build.try_compile("abc")?;

    // generate bindings to abc headers
    let abc_header = abc_dir.join("base").join("main").join("abcapis.h");
    bindgen::Builder::default()
        .header(format!("{}", abc_header.display()))
        .clang_arg(format!("-I{}", abc_dir.display()))
        .clang_arg(format!("-I{}", aiger_dir.display()))
        .clang_arg(format!("-D{}", lin))
        .generate()
        .map_err(|()| BuildError::BindgenError)?
        .write_to_file(build_env.out_dir.join("abc_bindings.rs"))?;

    // link to abc
    println!("cargo:rustc-link-lib=static=abc");

    Ok(())
}
