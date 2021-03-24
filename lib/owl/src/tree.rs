//! Valuation trees for querying and iterating over successors.

use std::convert::TryFrom;
use std::ops::Index;

use cudd::{Cudd, BDD};

/// An index for a node of a tree.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct TreeIndex(pub(crate) usize);

/// The type for identifying a variable in a valuation.
type TreeVar = usize;

impl std::fmt::Display for TreeIndex {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl TreeIndex {
    /// The index for the root node of any tree.
    pub const ROOT: Self = Self(0);
}

/// An inner node of a tree.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct InnerNode {
    /// The variable which is evaluated at this node.
    var: TreeVar,
    /// The successor if the variable is false in the valuation.
    left: TreeIndex,
    /// The successor if the variable is true in the valuation.
    right: TreeIndex,
}

impl InnerNode {
    /// The variable which is evaluated at this inner node.
    pub const fn var(&self) -> TreeVar {
        self.var
    }
}

/// A node of a valuation tree.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Node<T> {
    /// An inner node.
    Inner(InnerNode),
    /// A leaf node with a value of type `T`.
    Leaf(T),
}

impl<T> Node<T> {
    /// Create a new inner node with the given variable, left successor and right successor.
    pub(crate) const fn new_inner(var: TreeVar, left: TreeIndex, right: TreeIndex) -> Self {
        Self::Inner(InnerNode { var, left, right })
    }
    /// Create a new leaf node with the given value.
    pub(crate) const fn new_leaf(value: T) -> Self {
        Self::Leaf(value)
    }
    /// Returns `true` if this node is an inner node.
    pub const fn is_inner(&self) -> bool {
        matches!(self, Self::Inner(_))
    }
    /// Returns `true` if this node is a leaf.
    pub const fn is_leaf(&self) -> bool {
        matches!(self, Self::Leaf(_))
    }
}

/// A valuation tree, which is a compact representation of mapping
/// variable valuations to values.
///
/// Valuation trees can be indexed by a [`TreeIndex`], returning a
/// reference to a [`Node`]. The leaf value for a variable valuation
/// can be obtained with [`ValuationTree::lookup`].
/// The indices of certain nodes in the tree can be obtained
/// with [`ValuationTree::index_iter`].
#[derive(Clone, Debug)]
pub struct ValuationTree<T> {
    /// The vector of nodes, to be indexed by a tree index.
    tree: Vec<Node<T>>,
}

impl<T> ValuationTree<T> {
    /// Creates a new valuation tree with a single leaf containing
    /// the given value, which is then also the root node.
    pub(crate) fn single(value: T) -> Self {
        Self {
            tree: vec![Node::new_leaf(value)],
        }
    }

    /// Creates a new valuation tree from the given vector of nodes,
    /// without checking validity of the nodes.
    ///
    /// It is the callers responsibility to guarantee that all successor indices,
    /// starting from the root node, point to valid nodes, and that the induced
    /// successor graph is acyclic, i.e. every path eventually reaches a leaf node.
    /// Further, it is necessary that variables appear in increasing order of their
    /// index along any path.
    pub(crate) fn new_unchecked(tree: Vec<Node<T>>) -> Self {
        Self { tree }
    }

    /// Returns the number of nodes in the tree.
    fn size(&self) -> usize {
        self.tree.len()
    }

    /// Returns a reference to the value stored in the leaf
    /// of this tree for the given valuation.
    pub fn lookup<'a>(&'a self, valuation: &[bool]) -> &'a T {
        let mut index = TreeIndex::ROOT;
        loop {
            match &self[index] {
                Node::Inner(node) => {
                    if valuation[node.var] {
                        index = node.right;
                    } else {
                        index = node.left;
                    }
                }
                Node::Leaf(value) => return value,
            }
        }
    }

    /// Returns an iterator over all the tree indices reached
    /// from the node with the given source index until either
    /// a leaf is reached, or an inner node with a variable index
    /// greater or equal than the given target variable is reached,
    /// if such a variable is given.
    ///
    /// The reached target leaves and/or nodes are included in the iterator.
    #[must_use]
    pub fn index_iter(
        &self,
        source: TreeIndex,
        target_var: Option<TreeVar>,
    ) -> TreeIndexIterator<T> {
        TreeIndexIterator::new(self, source, target_var)
    }

    /// Returns a BDD for the valuations along all paths from the node
    /// with the given source index until the node with the given target index
    /// is reached.
    ///
    /// If `target_var` is given, only paths are used that contain no successor of an inner
    /// node with a variable index greater or equal than the target variable.
    ///
    /// The variable used for the BDD nodes is equal to the variable indices of the inner
    /// nodes along the paths plus the given `shift_var` value.
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

    /// Recursive implementation to obtain the BDDs for [`Self::bdd_for_paths`], with an additional
    /// cache of BDDs for already visited nodes.
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
                Node::Inner(node) => {
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
                Node::Leaf(_) => manager.bdd_zero(),
            }
        }
    }
}

impl<T> Index<TreeIndex> for ValuationTree<T> {
    type Output = Node<T>;

    fn index(&self, index: TreeIndex) -> &Self::Output {
        &self.tree[index.0]
    }
}

/// An iterator over the indices of nodes in in a valuation tree,
/// constructed by [`ValuationTree::index_iter`].
pub struct TreeIndexIterator<'a, T> {
    /// Reference to the tree.
    tree: &'a ValuationTree<T>,
    /// Stack of nodes that we still need to visit.
    stack: Vec<TreeIndex>,
    /// Vector indicating which nodes we have already visited.
    visited: Vec<bool>,
    /// The target variable index from the original function call.
    target_var: Option<TreeVar>,
}

impl<'a, T> TreeIndexIterator<'a, T> {
    /// Creates a new tree index iterator with the given source and target variable.
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
                    Node::Inner(node) => match self.target_var {
                        Some(v) if node.var >= v => return Some(index),
                        _ => {
                            self.stack.push(node.right);
                            self.stack.push(node.left);
                        }
                    },
                    Node::Leaf(_) => {
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

        let n0 = Node::new_inner(0, t1, t2);
        let n1 = Node::new_inner(1, t6, t3);
        let n2 = Node::new_inner(1, t3, t4);
        let n3 = Node::new_inner(2, t6, t7);
        let n4 = Node::new_inner(2, t5, t9);
        let n5 = Node::new_inner(3, t7, t8);
        let n6 = Node::new_leaf("a");
        let n7 = Node::new_leaf("b");
        let n8 = Node::new_leaf("c");
        let n9 = Node::new_leaf("d");

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
