use std::collections::HashMap;
use std::fmt;

use aiger::{AigerConstructor, Literal};
use cudd::{BddView, Cudd, ReorderingMethod, BDD};
use log::info;

use super::aiger::AigerController;

pub struct BddController {
    inputs: Vec<String>,
    outputs: Vec<String>,
    state_names: Vec<String>,
    initial_state: Vec<bool>,
    state_bdds: Vec<BDD>,
    output_bdds: Vec<BDD>,
    manager: Cudd,
}

impl BddController {
    pub(super) fn new(
        inputs: Vec<String>,
        outputs: Vec<String>,
        initial_state: Vec<bool>,
        state_bdds: Vec<BDD>,
        output_bdds: Vec<BDD>,
        mut manager: Cudd,
    ) -> Self {
        let state_names = (0..initial_state.len())
            .map(|i| format!("l{}", i))
            .collect();
        // ensure that dynamic reordering is disabled for a later consistent traversal of the BDDs
        manager.autodyn_disable();
        Self {
            inputs,
            outputs,
            state_names,
            initial_state,
            state_bdds,
            output_bdds,
            manager,
        }
    }

    fn num_state_vars(&self) -> usize {
        self.initial_state.len()
    }

    pub(crate) fn num_bdd_vars(&self) -> usize {
        self.inputs.len() + self.num_state_vars()
    }

    fn bdd_to_aig(
        mut aig: &mut AigerConstructor,
        bdd: &BDD,
        mut bdd_cache: &mut HashMap<BDD, Literal>,
        input_state_lits: &[Literal],
    ) -> Literal {
        let node = bdd.regular();
        let literal = bdd_cache.get(&node).cloned().unwrap_or_else(|| {
            let lit = match bdd.view() {
                BddView::Constant => Literal::TRUE,
                BddView::InnerNode {
                    var,
                    bdd_then,
                    bdd_else,
                } => {
                    let lit_var = input_state_lits[var];
                    let lit_then =
                        Self::bdd_to_aig(&mut aig, &bdd_then, &mut bdd_cache, input_state_lits);
                    let lit_else =
                        Self::bdd_to_aig(&mut aig, &bdd_else, &mut bdd_cache, input_state_lits);
                    aig.add_ite(lit_var, lit_then, lit_else)
                }
            };
            bdd_cache.insert(node, lit);
            lit
        });
        if bdd.is_complement() {
            !literal
        } else {
            literal
        }
    }

    pub(crate) fn create_aiger(&self) -> AigerController {
        info!("Creating aiger circuit from BDD");

        let mut aig = AigerConstructor::new(self.inputs.len(), self.num_state_vars()).unwrap();
        let mut input_state_lits = Vec::with_capacity(self.num_bdd_vars());
        for i in &self.inputs {
            input_state_lits.push(aig.add_input(i));
        }
        for s in &self.state_names {
            input_state_lits.push(aig.add_latch(s));
        }

        let mut cache = HashMap::new();
        for (o, output_bdd) in self.outputs.iter().zip(self.output_bdds.iter()) {
            let lit = Self::bdd_to_aig(&mut aig, output_bdd, &mut cache, &input_state_lits);
            aig.add_output(o, lit);
        }
        let state_lits = &input_state_lits[self.inputs.len()..];
        for ((&state_init, state_bdd), &state_lit) in self
            .initial_state
            .iter()
            .zip(self.state_bdds.iter())
            .zip(state_lits.iter())
        {
            let lit = Self::bdd_to_aig(&mut aig, state_bdd, &mut cache, &input_state_lits);
            aig.set_latch_next(state_lit, lit);
            aig.set_latch_reset(state_lit, Literal::from_bool(state_init));
        }

        AigerController::new(aig.into_aiger())
    }

    pub(crate) fn reduce(&mut self, exact: bool) {
        info!("Reducing BDD by variable reordering");
        let reordering_type = if exact {
            ReorderingMethod::Exact
        } else {
            ReorderingMethod::SiftConverge
        };
        self.manager.reduce_heap(reordering_type, 0);
    }
}

impl fmt::Display for BddController {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut bdds = Vec::with_capacity(self.output_bdds.len() + self.state_bdds.len());
        bdds.extend(self.output_bdds.iter().cloned());
        bdds.extend(self.state_bdds.iter().cloned());

        let mut in_names = Vec::with_capacity(self.inputs.len() + self.num_state_vars());
        in_names.extend(self.inputs.iter().cloned());
        in_names.extend(self.state_names.iter().cloned());

        let mut out_names = Vec::with_capacity(self.outputs.len() + self.num_state_vars());
        out_names.extend(self.outputs.iter().cloned());
        out_names.extend(self.state_names.iter().cloned());

        let dot = self.manager.dump_dot(&bdds, &in_names, &out_names);
        write!(f, "{}", dot)
    }
}
