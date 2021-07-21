# Changelog

## 21.0.0

### Changed

- Complete rewrite of Strix in Rust.
- Use new LTL-to-DPA translation in Owl based on Zielonka split-trees and
  [Alternating Cycle Decomposition (ACD)](https://arxiv.org/abs/2011.13041).
- Output of controller as machine now uses the [HOA format](https://adl.github.io/hoaf/) instead of KISS format.

### Added

- Additional machine minimization better exploiting non-determinism, can be controlled with option `--minimize`.
- Enable usage of structured labels on minimized machines.
- Option `--solver` to use different parity game solvers:
    [FPI](https://arxiv.org/abs/1909.07659),
    Zielonka's algorithm and [Strategy Iteration](https://arxiv.org/abs/0806.2923),
    with FPI as the default.
    Zielonka's algorithm can currently only be used for checking realizability.
- Option `--onthefly` to control interleaving of solver with on-the-fly exploration.
- Option `--lookahead` to control application of Zielonka tree and ACD construction.

### Removed

- HOA input for solving parity games in extended HOA format.
- Filtering of exploration queue based on reachable border nodes.
- Declaration of winning and losing states for realizability propagation in DPA.

### Internal

- Move project structure to GitHub.
- Closer integration of bundled ABC library to reduce build size and time.
- Verification tests now have a different but reduced set of dependencies.
