//! Parity games.

use std::collections::hash_map::Entry;
use std::collections::HashMap;
use std::collections::VecDeque;
use std::fmt;
use std::hash::Hash;
use std::io;
use std::ops::{Index, IndexMut};

use fixedbitset::FixedBitSet;

use owl::automaton::Color;

use super::Parity;

/// A player in a parity game.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Player {
    /// Player with max-even winning condition.
    Even = 0,
    /// Player with max-odd winning condition.
    Odd = 1,
}

impl std::ops::Not for Player {
    type Output = Self;

    fn not(self) -> Self::Output {
        match self {
            Self::Even => Self::Odd,
            Self::Odd => Self::Even,
        }
    }
}

impl fmt::Display for Player {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        let string = match self {
            Self::Even => "even",
            Self::Odd => "odd",
        };
        write!(f, "{}", string)
    }
}

impl Player {
    pub(crate) const PLAYERS: [Self; 2] = [Self::Even, Self::Odd];
}

impl From<Player> for u32 {
    fn from(player: Player) -> Self {
        match player {
            Player::Even => 0,
            Player::Odd => 1,
        }
    }
}

impl From<Parity> for Player {
    fn from(p: Parity) -> Self {
        match p {
            Parity::Even => Self::Even,
            Parity::Odd => Self::Odd,
        }
    }
}

impl From<Player> for Parity {
    fn from(p: Player) -> Self {
        match p {
            Player::Even => Self::Even,
            Player::Odd => Self::Odd,
        }
    }
}

/// The type for an index of a node in a parity game.
pub type NodeIndex = usize;

/// A labelled node in a parity game.
pub trait Node {
    /// The type of the label for a node.
    type Label;

    /// Returns the owner controlling this node.
    fn owner(&self) -> Player;
    /// Returns the color of this node.
    fn color(&self) -> Color;
    /// Returns the label of this node.
    fn label(&self) -> &Self::Label;
    /// Returns the indices of successors of this node.
    fn successors(&self) -> &[NodeIndex];
    /// Returns the indices of predecessors of this node.
    fn predecessors(&self) -> &[NodeIndex];

    /// Returns the parity of the color of this node.
    fn parity(&self) -> Parity {
        Parity::of(self.color())
    }
}

/// A parity game.
pub trait Game<'a>: Index<NodeIndex, Output = <Self as Game<'a>>::Node> {
    /// The type of nodes for this parity game.
    type Node: Node;
    /// The type for the iterator returned by [`Self::nodes`].
    type NodeIndexIterator: Iterator<Item = NodeIndex> + 'a;
    /// The type for the iterator returned by [`Self::nodes_with_color`].
    type NodesWithColorIterator: Iterator<Item = NodeIndex> + 'a;

    /// Returns the index of the initial node of the parity game,
    /// from which any play is required to start.
    fn initial_node(&self) -> NodeIndex;
    /// Returns the number of nodes in this parity game.
    ///
    /// All indices of nodes in the game will be less than this number.
    fn num_nodes(&self) -> NodeIndex;
    /// Returns the number of colors in this parity game.
    ///
    /// Any color of a node in this game will be less than this number.
    fn num_colors(&self) -> Color;
    /// Returns an iterator over the indices of nodes in this parity game.
    fn nodes(&'a self) -> Self::NodeIndexIterator;
    /// Returns an iterator over the indices of nodes that have the given color.
    ///
    /// The returned iterator may yield no nodes if there is no node with that color.
    fn nodes_with_color(&'a self, color: Color) -> Self::NodesWithColorIterator;

    /// Returns the border region of this parity game, which are nodes that have
    /// no successors and should be treated as losing for both players once a play
    /// reaches such a node.
    ///
    /// Nodes in the border have an owner and a color, which are however implementation-defined
    /// and should not be used. Once a node is updated and removed from the border,
    /// the owner and color can change to their proper value.
    fn border(&self) -> &Region;
}

/// A region of a parity game, defining a set of nodes of the game in this region.
///
/// A region can be indexed by the index of a game node, which returns `true` if
/// the node is in that region.
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Region {
    data: FixedBitSet,
}

impl Index<NodeIndex> for Region {
    type Output = bool;

    fn index(&self, index: NodeIndex) -> &Self::Output {
        &self.data[index]
    }
}

impl std::fmt::Display for Region {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{{")?;
        for index in self.data.ones() {
            write!(f, " {}", index)?;
        }
        write!(f, " }}")?;
        Ok(())
    }
}

impl Region {
    pub(crate) fn new() -> Self {
        Self {
            data: FixedBitSet::default(),
        }
    }

    pub(crate) fn with_capacity(n: usize) -> Self {
        Self {
            data: FixedBitSet::with_capacity(n),
        }
    }

    pub(crate) fn nodes(&self) -> fixedbitset::Ones {
        self.data.ones()
    }

    pub(crate) fn grow(&mut self, n: usize) {
        self.data.grow(n);
    }

    pub(crate) fn union_with(&mut self, other: &Self) {
        self.data.union_with(&other.data);
    }

    pub(crate) fn union(&self, other: &Self) -> Self {
        let mut new_region = self.clone();
        new_region.union_with(other);
        new_region
    }

    pub(crate) fn insert(&mut self, index: NodeIndex) {
        self.data.insert(index);
    }

    pub(crate) fn set(&mut self, index: NodeIndex, value: bool) {
        self.data.set(index, value);
    }

    pub(crate) fn size(&self) -> usize {
        self.data.count_ones(..)
    }

    pub(crate) fn attract<'a, G: Game<'a>>(&self, game: &'a G, player: Player) -> Self {
        let mut region = self.clone();
        region.attract_mut(game, player);
        region
    }

    pub(crate) fn attract_mut<'a, G: Game<'a>>(&mut self, game: &'a G, player: Player) {
        let n = game.num_nodes();
        let mut count: Vec<isize> = vec![-1; n];
        let mut queue = VecDeque::with_capacity(n);
        queue.extend(self.nodes());
        while let Some(i) = queue.pop_front() {
            for &j in game[i].predecessors() {
                if !self[j] {
                    let controllable = player == game[j].owner();
                    if !controllable {
                        if count[j] == -1 {
                            count[j] = game[j].successors().len() as isize;
                        }
                        count[j] -= 1;
                    }
                    if controllable || count[j] == 0 {
                        self.insert(j);
                        queue.push_back(j);
                    }
                }
            }
        }
    }

    pub(crate) fn attract_mut_without<'a, G: Game<'a>>(
        &mut self,
        game: &'a G,
        player: Player,
        disabled: &Self,
    ) -> bool {
        let n = game.num_nodes();
        let mut count: Vec<isize> = vec![-1; n];
        let mut queue = VecDeque::with_capacity(n);
        let mut change = false;
        queue.extend(self.nodes());
        while let Some(i) = queue.pop_front() {
            for &j in game[i].predecessors().iter().filter(|&&j| !disabled[j]) {
                if !self[j] {
                    let controllable = player == game[j].owner();
                    if !controllable {
                        if count[j] == -1 {
                            count[j] = game[j]
                                .successors()
                                .iter()
                                .filter(|&&k| !disabled[k])
                                .count() as isize;
                        }
                        count[j] -= 1;
                    }
                    if controllable || count[j] == 0 {
                        change = true;
                        self.insert(j);
                        queue.push_back(j);
                    }
                }
            }
        }
        change
    }
}

impl std::iter::Extend<NodeIndex> for Region {
    fn extend<T: IntoIterator<Item = NodeIndex>>(&mut self, iter: T) {
        self.data.extend(iter)
    }
}

/// A labelled node of [`LabelledGame<L>`].
#[derive(Debug)]
pub struct LabelledNode<L> {
    successors: Vec<NodeIndex>,
    predecessors: Vec<NodeIndex>,
    owner: Player,
    color: Color,
    label: L,
}

impl<L> LabelledNode<L> {
    pub(crate) fn new(owner: Player, color: Color, label: L) -> Self {
        Self {
            successors: Vec::new(),
            predecessors: Vec::new(),
            owner,
            color,
            label,
        }
    }
    fn new_unexplored(label: L) -> Self {
        Self::new(Player::Even, 0, label)
    }
}

impl<L> Node for LabelledNode<L> {
    type Label = L;

    fn owner(&self) -> Player {
        self.owner
    }
    fn color(&self) -> Color {
        self.color
    }
    fn label(&self) -> &Self::Label {
        &self.label
    }
    fn successors(&self) -> &[NodeIndex] {
        &self.successors
    }
    fn predecessors(&self) -> &[NodeIndex] {
        &self.predecessors
    }
}

/// A parity game with labelled nodes.
#[derive(Debug)]
pub struct LabelledGame<L> {
    nodes: Vec<LabelledNode<L>>,
    mapping: HashMap<L, NodeIndex>,
    border: Region,
    color_map: Vec<Vec<NodeIndex>>,
    initial_node: Option<NodeIndex>,
}

impl<L: Hash + Eq + Clone> Default for LabelledGame<L> {
    fn default() -> Self {
        Self {
            nodes: Vec::with_capacity(4096),
            mapping: HashMap::with_capacity(4096),
            border: Region::with_capacity(256),
            color_map: Vec::with_capacity(4096),
            initial_node: None,
        }
    }
}

impl<L: Hash + Eq + Clone> LabelledGame<L> {
    pub(crate) fn set_initial_node(&mut self, index: NodeIndex) {
        self.initial_node = Some(index);
    }

    pub(crate) fn add_border_node(&mut self, label: L) -> (NodeIndex, bool) {
        match self.mapping.entry(label) {
            Entry::Occupied(entry) => (*entry.get(), false),
            Entry::Vacant(entry) => {
                // new node
                let game_node = LabelledNode::new_unexplored(entry.key().clone());
                let index = self.nodes.len();
                self.nodes.push(game_node);
                self.border.grow(index + 1);
                self.border.insert(index);
                entry.insert(index);
                (index, true)
            }
        }
    }

    /// Add a new node with the given label, owner and color, and returns the node index.
    ///
    /// # Panics
    ///
    /// Panics if a node with the given label is already present.
    #[cfg(test)]
    fn add_node(&mut self, label: L, owner: Player, color: Color) -> NodeIndex {
        let (index, new_node) = self.add_border_node(label);
        assert!(new_node);
        self.update_node(index, owner, color);
        index
    }
}

impl<L> LabelledGame<L> {
    pub(crate) fn update_node(&mut self, index: NodeIndex, owner: Player, color: Color) {
        assert!(self.border[index]);
        self.border.set(index, false);
        let node = &mut self[index];
        node.owner = owner;
        node.color = color;
        if color >= self.num_colors() {
            self.color_map.resize(color + 1, Vec::new());
        }
        self.color_map[color].push(index);
    }

    pub(crate) fn add_edge(&mut self, from: NodeIndex, to: NodeIndex) {
        self[from].successors.push(to);
        self[to].predecessors.push(from);
    }
}

impl<'a, L> Game<'a> for LabelledGame<L> {
    type Node = LabelledNode<L>;
    type NodeIndexIterator = std::ops::Range<NodeIndex>;
    type NodesWithColorIterator = std::iter::Cloned<std::slice::Iter<'a, NodeIndex>>;

    fn initial_node(&self) -> NodeIndex {
        self.initial_node.expect("no initial node")
    }

    fn num_nodes(&self) -> NodeIndex {
        self.nodes.len()
    }

    fn num_colors(&self) -> Color {
        self.color_map.len()
    }

    fn nodes(&self) -> Self::NodeIndexIterator {
        0..self.nodes.len()
    }

    fn nodes_with_color(&'a self, color: Color) -> Self::NodesWithColorIterator {
        self.color_map[color].iter().cloned()
    }

    fn border(&self) -> &Region {
        &self.border
    }
}

impl<L> Index<NodeIndex> for LabelledGame<L> {
    type Output = LabelledNode<L>;

    fn index(&self, index: NodeIndex) -> &Self::Output {
        &self.nodes[index]
    }
}

impl<L> IndexMut<NodeIndex> for LabelledGame<L> {
    fn index_mut(&mut self, index: NodeIndex) -> &mut Self::Output {
        &mut self.nodes[index]
    }
}

/// Helper struct to display a parity game with different options
/// for assigning the border to a player.
struct GameDisplay<'a, G> {
    game: &'a G,
    winner: Option<Player>,
}

impl<'a, G: Game<'a>> fmt::Display for GameDisplay<'a, G>
where
    <G::Node as Node>::Label: fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "parity {};", self.game.num_nodes())?;
        for i in self.game.nodes() {
            let node = &self.game[i];
            if self.game.border()[i] {
                match self.winner {
                    Some(p) => write!(
                        f,
                        "{} {} {} {}",
                        i,
                        Color::from(Parity::from(!p)),
                        u32::from(!p),
                        i
                    )?,
                    None => write!(f, "{}", i)?,
                };
                write!(f, " \"{} (border)\"", node.label())?;
            } else {
                write!(f, "{} {} {} ", i, node.color(), u32::from(node.owner()))?;
                for (j, succ) in node.successors().iter().enumerate() {
                    if j > 0 {
                        write!(f, ",")?;
                    }
                    write!(f, "{}", succ)?;
                }
                write!(f, " \"{}\"", node.label())?;
            }
            writeln!(f, ";")?;
        }
        Ok(())
    }
}

impl<L: fmt::Display> LabelledGame<L> {
    pub(crate) fn write_with_winner<W: io::Write>(
        &self,
        mut writer: W,
        winner: Player,
    ) -> io::Result<()> {
        write!(
            writer,
            "{}",
            GameDisplay {
                game: self,
                winner: Some(winner)
            }
        )
    }
}

impl<L: fmt::Display> fmt::Display for LabelledGame<L> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{}",
            GameDisplay {
                game: self,
                winner: None
            }
        )
    }
}

/// Tests for parity games.
#[cfg(test)]
mod tests {
    use super::*;

    /// Test attractor computation on a parity game.
    #[test]
    fn test_attractor() {
        let mut game = LabelledGame::default();

        let n0 = game.add_node(0, Player::Odd, 0);
        let n1 = game.add_node(1, Player::Even, 1);
        let n2 = game.add_node(2, Player::Even, 2);
        let n3 = game.add_node(3, Player::Odd, 3);
        let n4 = game.add_node(4, Player::Odd, 4);
        let n5 = game.add_node(5, Player::Even, 5);
        let (n6, _) = game.add_border_node(6);

        game.add_edge(n0, n1);
        game.add_edge(n0, n2);
        game.add_edge(n1, n0);
        game.add_edge(n1, n3);

        game.add_edge(n2, n2);
        game.add_edge(n2, n4);
        game.add_edge(n3, n3);
        game.add_edge(n3, n5);

        game.add_edge(n4, n5);
        game.add_edge(n4, n6);
        game.add_edge(n5, n4);
        game.add_edge(n5, n6);

        let attractor_even = game.border().attract(&game, Player::Even);
        let attractor_odd = game.border().attract(&game, Player::Odd);

        assert!(!attractor_even[n0]);
        assert!(!attractor_odd[n0]);
        assert!(!attractor_even[n1]);
        assert!(!attractor_odd[n1]);
        assert!(attractor_even[n2]);
        assert!(!attractor_odd[n2]);
        assert!(!attractor_even[n3]);
        assert!(attractor_odd[n3]);
        assert!(attractor_even[n4]);
        assert!(attractor_odd[n4]);
        assert!(attractor_even[n5]);
        assert!(attractor_odd[n5]);
        assert!(attractor_even[n6]);
        assert!(attractor_odd[n6]);
    }
}
