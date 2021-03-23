use build_util::*;

fn main() {
    run_build_or_exit(build, "aiger");
}

fn build() -> Result<(), BuildError> {
    let build_env = fetch_env()?;
    let aiger_dir = build_env.root_dir.join("c");

    // locate source and header
    let aiger_source = aiger_dir.join("aiger.c");
    let aiger_header = aiger_dir.join("aiger.h");

    // build aiger
    let mut build = cc::Build::new();
    build.warnings(false);
    if build_env.profile == Profile::Release {
        build.define("NDEBUG", None);
    }
    build.file(&aiger_source);
    build.try_compile("aiger")?;

    // generate bindings to aiger headers
    bindgen::Builder::default()
        .header(format!("{}", aiger_header.display()))
        .generate()
        .map_err(|()| BuildError::Bindgen)?
        .write_to_file(build_env.out_dir.join("aiger_bindings.rs"))?;

    // link to aiger
    println!("cargo:rustc-link-lib=static=aiger");

    // invalidate the built crate when any file of aiger changes
    println!("cargo:rerun-if-changed={}", aiger_source.display());
    println!("cargo:rerun-if-changed={}", aiger_header.display());

    Ok(())
}
