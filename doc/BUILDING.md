# Building

## Dependencies

 - [Rust](https://www.rust-lang.org/) toolchain in version 1.50.0 or higher.
 - C compiler, e.g. [GCC](https://gcc.gnu.org/).
 - [Clang](https://clang.llvm.org/) libraries (`libclang`) to generate bindings to C libraries.
 - [zlib](https://www.zlib.net/) in version 1.2.7 or higher.
 - JDK 11 or higher, e.g. [OpenJDK](https://openjdk.java.net/).
 - [GraalVM](https://www.graalvm.org/) for JDK 11 or higher with `native-image` binary.

## Compilation

If the repository has been cloned with git, first the submodules have to be initialized as follows:
```
git submodule init
git submodule update
```
If Strix has been obtained as a release zip file, this step can be skipped.

The compilation process can be started as follows for the release build:
```
cargo build --release
```
Strix can then be run as follows:
```
cargo run --release -- [OPTIONS]
```

## Build binary distribution

To build a binary distribution, the following command can be used:
```
cargo dist build
```
Afterwards, the folder `target/dist` will be created and should contain the executable `strix` and the library `libowl.so`.
To use Strix, the executable `strix` has to be invoked with `libowl.so` in the library path.

To create and install a package for Ubuntu or Debian, use the following commands:
```
cargo dist build-deb
sudo dpkg -i target/dist/strix-*.deb
```
To create and install a package for Arch Linux or Manjaro, use the following commands:
```
cargo dist build-pkg
sudo pacman -U target/dist/strix-*.pkg.tar.zst
```

## Test dependencies

Execution of all tests requires additional dependencies to verify correctness of controllers:

- [Spot](https://spot.lrde.epita.fr/) with `ltlfilt`, `ltl2tgba` and `autfilt` binaries.
- [NuSMV](https://nusmv.fbk.eu/) with `ltl2smv` binary.
- [AIGER](https://github.com/arminbiere/aiger) tools with `smvtoaig` binary.
- [combine-aiger](https://github.com/meyerphi/combine-aiger.git).
- [nuXmv](https://nuxmv.fbk.eu/) in version 2.0.0.

The repository includes [a script](../scripts/install_verification_tools.sh) that installs these dependencies for the CI, which can be adapted for local installation.

The test suite can then be run as follows:
```
cargo test
```
