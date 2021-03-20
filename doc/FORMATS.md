# Formats

## Input Formats

A synthesis specification can be given to strix by an LTL formula together
with a splitting of propositions into output and input propositions. A full
description of the supported LTL formulas is given in the
[Owl formats description](https://gitlab.lrz.de/i7/owl/blob/master/doc/FORMATS.md).
To invoke Strix like this, give the formula and propositions by
```
strix --formula <FORMULA> --ins=<INPUTS> --outs=<OUTPUTS>
```
where `<FORMULA>` is the LTL formula, `<INPUTS>` a comma-separated list of input propositions
and `<OUTPUTS>` a comma-separated list of output propositions. The propositions in both lists
should be a partition of all atomic propositions appearing in `<FORMULA>`.
Instead of `--formula` also the short option `-f` can be used.
The LTL formula can be given in a file `<FILE>` as follows:
```
strix -F <FILE> --ins=<INPUTS> --outs=<OUTPUTS>
```

For example, a controller for a simple arbiter specification can be synthesized as follows:
```
strix -f "G (!grant0 | !grant1) & G (req0 -> F grant0) & G (req1 -> F grant1)" --ins="req0,req1" --outs="grant0,grant1"
```

Strix has no native support for TLSF specifications, but these can be used
after conversion with the [SyfCo](https://github.com/reactive-systems/syfco) tool.

## Output Formats

Strix supports the following output formats, which is controlled by the `-o <ARG>` option, where `<ARG>` is one of `hoa`,`aag`,`aig`,`bdd` or `pg`:

- Mealy or Moore machine ([HOA format](http://adl.github.io/hoaf/))
- AIGER circuit ([AIGER format](https://github.com/arminbiere/aiger) wit AAG (ASCII) and AIG (binary) option)
- BDD ([DOT format](https://graphviz.org/) with [CUDD interpretation](http://web.mit.edu/sage/export/tmp/y/usr/share/doc/polybori/cudd/node3.html#SECTION000318000000000000000))
- Parity game ([PGSolver format](https://www.win.tue.nl/~timw/downloads/amc2014/pgsolver.pdf))

For any specification, Strix first outputs the realizability header, which is either `REALIZABLE` or `UNREALIZABLE`.
Then, if the option `--realizability` is not given,
the output of the controller in one of the above formats follows.
By default, the controller is written to the standard output,
but can be redirected to a file by specifying the option `-O <OUTPUT>`, where `<OUTPUT>` is the output file name.
