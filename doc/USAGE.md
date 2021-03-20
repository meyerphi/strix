# Usage

Strix accepts specifications as LTL formulas, for more details see the [formats description](FORMATS.md).

To check realizability of an LTL formula `LTL_FORMULA` together with two
comma-separated lists of input and output propositions `INS` and `OUTS`,
Strix should be invoked as follows:
```
strix --realizability -f "LTL_FORMULA" --ins="INS" --outs="OUTS"
```
To output a controller as a Mealy machine in the default HOA format, use:
```
strix -f "LTL_FORMULA" --ins="INS" --outs="OUTS"
```
To output a controller as an AIGER circuit with several minimization techniques, simply use:
```
strix --aiger -f "LTL_FORMULA" --ins="INS" --outs="OUTS"
```
To fine-tune the construction of either the Mealy machine or the circuit, only specify the output format using `-o` and add additional options, e.g. as follows to use a structured encoding of the state labels:
```
strix -o hoa -f "LTL_FORMULA" --ins="INS" --outs="OUTS" -l structured
strix -o aag -f "LTL_FORMULA" --ins="INS" --outs="OUTS" -l structured
```
Strix has many more options, to list them use `strix --help`.

## Example

A simple arbiter with two clients can be synthesized as follows:
```
strix -f "G (req0 -> F grant0) & G (req1 -> F grant1) & G (!(grant0 & grant1))" --ins="req0,req1" --outs="grant0,grant1"
```

## TLSF

To use Strix with [TLSF](https://arxiv.org/abs/1604.02284) specifications, a [wrapper script](../scripts/strix_tlsf.sh) is provided, which assumes
that the [SyfCo](https://github.com/reactive-systems/syfco) tool is installed and which may be called as follows:
```
scripts/strix_tlsf.sh TLSF_INPUT.tlsf [OPTIONS]
```
