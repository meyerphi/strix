//! Build script for owl crate.

use std::process;
use walkdir::WalkDir;

use build_util::*;

fn main() {
    run_build_or_exit(build, "Owl");
}

/// Run the build for Owl, by invoking the gradle build for the bundled Java library.
fn build() -> Result<(), BuildError> {
    let build_env = fetch_env()?;
    let owl_dir = build_env.root_dir.join("owl");
    let out_dir = build_env.out_dir;

    let gradlew_script = if cfg!(target_os = "windows") {
        "gradlew.bat"
    } else {
        "gradlew"
    };
    let gradlew = owl_dir.join(gradlew_script);
    let cache_dir = out_dir.join(".gradle");
    let link_script = if cfg!(target_os = "windows") {
        "link_static_lib.cmd"
    } else {
        "link_static_lib.sh"
    };
    let link_script_path = build_env.root_dir.join("scripts").join(link_script);

    let mut gradlew_cmd = process::Command::new(gradlew);
    gradlew_cmd.env(
        "GRADLE_OPTS",
        &format!("-Dorg.gradle.project.buildDir={}", out_dir.display()),
    );
    // pass custom linker to build static library
    gradlew_cmd.env("CC", &format!("{}", link_script_path.display()));
    // options to build Owl in target directory
    gradlew_cmd.args(&[
        "distZip",
        "-Pdisable-pandoc",
        &format!("-p{}", owl_dir.display()),
        &format!("--project-cache-dir={}", cache_dir.display()),
        "--no-configuration-cache",
    ]);
    if build_env.profile == Profile::Debug {
        gradlew_cmd.arg("-Penable-native-assertions");
    }
    run_command(gradlew_cmd)?;

    // locate headers
    let owl_native_dir = out_dir.join("native-library");
    let owl_header_dir = owl_dir.join("src").join("main").join("c").join("headers");

    let graal_header = owl_native_dir.join("graal_isolate.h");
    let libowl_header = owl_native_dir.join("libowl.h");
    let owltypes_header = owl_header_dir.join("owltypes.h");

    // patch invalid and missing includes in header generated by native-image
    let libowl_patched_header = owl_native_dir.join("libowl_patched.h");
    // stddef.h needed for size_t by C standard
    // graal_isolate.h needs to have quotes instead of brackets
    let pattern = "#include <graal_isolate.h>";
    let replacements = ["#include <stddef.h>", "#include \"graal_isolate.h\""];
    patch_file(
        &libowl_header,
        &libowl_patched_header,
        pattern,
        &replacements,
    )?;

    // generate bindings to Owl headers
    bindgen::Builder::default()
        .header(format!("{}", owltypes_header.display()))
        .header(format!("{}", graal_header.display()))
        .header(format!("{}", libowl_patched_header.display()))
        .generate()
        .map_err(|()| BuildError::Bindgen)?
        .write_to_file(out_dir.join("owl_bindings.rs"))?;

    // link to Owl static library
    println!("cargo:rustc-link-lib=static=owl");
    // On Linux and macOS, GraalVM image needs zlib dependency
    if cfg!(any(target_os = "linux", target_os = "macos")) {
        println!("cargo:rustc-link-lib=dylib=z");
    }
    // On macOS it also needs the Foundation framework
    if cfg!(target_os = "macos") {
        println!("cargo:rustc-link-lib=framework=Foundation");
    }

    // set search path
    println!(
        "cargo:rustc-link-search=native={}",
        owl_native_dir.display()
    );

    // invalidate the built crate when any file in Owl source directory changes
    let owl_src_dir = owl_dir.join("src");
    for entry in WalkDir::new(owl_src_dir) {
        let entry = entry.map_err(std::io::Error::from)?;
        let path = entry.path();
        println!("cargo:rerun-if-changed={}", path.display());
    }
    println!("cargo:rerun-if-changed={}", link_script_path.display());

    Ok(())
}
