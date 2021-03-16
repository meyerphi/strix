use std::convert::TryFrom;
use std::ops::Index;

use cudd::{Cudd, BDD};

#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct TreeIndex(pub(crate) usize);

type TreeVar = usize;

impl std::fmt::Display for TreeIndex {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl TreeIndex {
    pub const ROOT: TreeIndex = TreeIndex(0);
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct InnerNode {
    var: TreeVar,
    left: TreeIndex,
    right: TreeIndex,
}

impl InnerNode {
    pub fn var(&self) -> TreeVar {
        self.var
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum TreeNode<T> {
    Node(InnerNode),
    Leaf(T),
}

impl<T> TreeNode<T> {
    pub fn new_node(var: TreeVar, left: TreeIndex, right: TreeIndex) -> TreeNode<T> {
        TreeNode::Node(InnerNode { var, left, right })
    }
    pub fn new_leaf(value: T) -> TreeNode<T> {
        TreeNode::Leaf(value)
    }
    pub fn is_node(&self) -> bool {
        matches!(self, TreeNode::Node(_))
    }
    pub fn is_leaf(&self) -> bool {
        matches!(self, TreeNode::Leaf(_))
    }
}

#[derive(Clone, Debug)]
pub struct ValuationTree<T> {
    tree: Vec<TreeNode<T>>,
}

impl<T> ValuationTree<T> {
    pub(crate) fn single(leaf: T) -> ValuationTree<T> {
        ValuationTree {
            tree: vec![TreeNode::new_leaf(leaf)],
        }
    }

    pub(crate) fn new_unchecked(tree: Vec<TreeNode<T>>) -> ValuationTree<T> {
        ValuationTree { tree }
    }

    /// Returns the total number of nodes of the tree.
    pub fn size(&self) -> usize {
        self.tree.len()
    }

    pub fn index_iter(
        &self,
        source: TreeIndex,
        target_var: Option<TreeVar>,
    ) -> TreeIndexIterator<T> {
        TreeIndexIterator::new(&self, source, target_var)
    }

    pub fn bdd_for_paths(
        &self,
        manager: &Cudd,
        source: TreeIndex,
        target: TreeIndex,
        target_var: Option<TreeVar>,
        shift_var: isize,
    ) -> BDD {
        let mut bdds = vec![None; self.size()];
        self.bdd_for_paths_rec(&mut bdds, manager, source, target, target_var, shift_var)
    }

    fn bdd_for_paths_rec(
        &self,
        bdds: &mut Vec<Option<BDD>>,
        manager: &Cudd,
        source: TreeIndex,
        target: TreeIndex,
        target_var: Option<TreeVar>,
        shift_var: isize,
    ) -> BDD {
        if source == target {
            manager.bdd_one()
        } else if let Some(bdd) = &bdds[source.0] {
            bdd.clone()
        } else {
            match &self[source] {
                TreeNode::Node(node) => {
                    if let Some(v) = target_var {
                        if node.var >= v {
                            return manager.bdd_zero();
                        }
                    }
                    let mut bdd =
                        manager.bdd_var(usize::try_from(node.var as isize + shift_var).unwrap());
                    let bdd_left = self
                        .bdd_for_paths_rec(bdds, manager, node.left, target, target_var, shift_var);
                    let bdd_right = self.bdd_for_paths_rec(
                        bdds, manager, node.right, target, target_var, shift_var,
                    );
                    bdd.ite_assign(&bdd_right, &bdd_left);
                    bdds[source.0] = Some(bdd.clone());
                    bdd
                }
                TreeNode::Leaf(_) => manager.bdd_zero(),
            }
        }
    }
}

impl<T> Index<TreeIndex> for ValuationTree<T> {
    type Output = TreeNode<T>;

    fn index(&self, index: TreeIndex) -> &Self::Output {
        &self.tree[index.0]
    }
}

pub struct TreeIndexIterator<'a, T> {
    tree: &'a ValuationTree<T>,
    stack: Vec<TreeIndex>,
    visited: Vec<bool>,
    target_var: Option<TreeVar>,
}

impl<'a, T> TreeIndexIterator<'a, T> {
    fn new(
        tree: &'a ValuationTree<T>,
        source: TreeIndex,
        target_var: Option<TreeVar>,
    ) -> TreeIndexIterator<'a, T> {
        let n = tree.size();
        let visited = vec![false; n];
        let mut stack = Vec::with_capacity(n);
        stack.push(source);
        TreeIndexIterator {
            tree,
            stack,
            visited,
            target_var,
        }
    }
}

impl<'a, T> Iterator for TreeIndexIterator<'a, T> {
    type Item = TreeIndex;

    fn next(&mut self) -> Option<Self::Item> {
        while let Some(index) = self.stack.pop() {
            if !self.visited[index.0] {
                self.visited[index.0] = true;
                match &self.tree[index] {
                    TreeNode::Node(node) => match self.target_var {
                        Some(v) if node.var >= v => return Some(index),
                        _ => {
                            self.stack.push(node.right);
                            self.stack.push(node.left);
                        }
                    },
                    TreeNode::Leaf(_) => {
                        return Some(index);
                    }
                }
            }
        }
        None
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, Some((self.tree.size() + 1) / 2))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_split() {
        let t0 = TreeIndex(0);
        let t1 = TreeIndex(1);
        let t2 = TreeIndex(2);
        let t3 = TreeIndex(3);
        let t4 = TreeIndex(4);
        let t5 = TreeIndex(5);
        let t6 = TreeIndex(6);
        let t7 = TreeIndex(7);
        let t8 = TreeIndex(8);
        let t9 = TreeIndex(9);

        assert_eq!(t0, TreeIndex::ROOT);

        let n0 = TreeNode::new_node(0, t1, t2);
        let n1 = TreeNode::new_node(1, t6, t3);
        let n2 = TreeNode::new_node(1, t3, t4);
        let n3 = TreeNode::new_node(2, t6, t7);
        let n4 = TreeNode::new_node(2, t5, t9);
        let n5 = TreeNode::new_node(3, t7, t8);
        let n6 = TreeNode::new_leaf("a");
        let n7 = TreeNode::new_leaf("b");
        let n8 = TreeNode::new_leaf("c");
        let n9 = TreeNode::new_leaf("d");

        let tree = vec![
            n0,
            n1,
            n2,
            n3,
            n4,
            n5,
            n6.clone(),
            n7.clone(),
            n8.clone(),
            n9.clone(),
        ];
        let tree = ValuationTree::new_unchecked(tree);

        let mut split: Vec<_> = tree.index_iter(TreeIndex::ROOT, Some(2)).collect();
        split.sort();
        assert_eq!(split, vec![t3, t4, t6]);

        let sub1: Vec<_> = tree.index_iter(t3, None).map(|i| &tree[i]).collect();
        assert_eq!(sub1, vec![&n6, &n7]);
        let sub2: Vec<_> = tree.index_iter(t4, None).map(|i| &tree[i]).collect();
        assert_eq!(sub2, vec![&n7, &n8, &n9]);
        let sub3: Vec<_> = tree.index_iter(t6, None).map(|i| &tree[i]).collect();
        assert_eq!(sub3, vec![&n6]);
    }
}
