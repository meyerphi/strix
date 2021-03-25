use std::collections::VecDeque;

use min_max_heap::MinMaxHeap;

pub(crate) trait ExplorationQueue<I, S> {
    fn push_scored(&mut self, item: I, score: S);
    fn push(&mut self, item: I);
    fn pop(&mut self) -> Option<I>;
}

pub(crate) struct BfsQueue<I> {
    queue: VecDeque<I>,
}

impl<I> BfsQueue<I> {
    pub(crate) fn with_capacity(capacity: usize) -> Self {
        Self {
            queue: VecDeque::with_capacity(capacity),
        }
    }
}

impl<I, S> ExplorationQueue<I, S> for BfsQueue<I> {
    fn push_scored(&mut self, item: I, _: S) {
        self.queue.push_back(item);
    }

    fn push(&mut self, item: I) {
        self.queue.push_front(item);
    }

    fn pop(&mut self) -> Option<I> {
        self.queue.pop_front()
    }
}

pub(crate) struct DfsQueue<I> {
    queue: Vec<I>,
}

impl<I> DfsQueue<I> {
    pub(crate) fn with_capacity(capacity: usize) -> Self {
        Self {
            queue: Vec::with_capacity(capacity),
        }
    }
}

impl<I, S> ExplorationQueue<I, S> for DfsQueue<I> {
    fn push_scored(&mut self, item: I, _: S) {
        self.queue.push(item);
    }

    fn push(&mut self, item: I) {
        self.queue.push(item);
    }

    fn pop(&mut self) -> Option<I> {
        self.queue.pop()
    }
}

#[derive(PartialEq, Eq, PartialOrd, Ord)]
struct ScoredItem<I, S> {
    score: S,
    item: I,
}

impl<I, S> ScoredItem<I, S> {
    const fn new(item: I, score: S) -> Self {
        Self { item, score }
    }
}

#[derive(Copy, Clone, PartialEq, Eq)]
pub(crate) enum MinMaxMode {
    Min,
    Max,
    MinMax,
}

pub(crate) struct MinMaxQueue<I, S> {
    direct_queue: Vec<I>,
    scored_queue: MinMaxHeap<ScoredItem<I, S>>,
    mode: MinMaxMode,
    next_max: bool,
}

impl<I: Ord, S: Ord> MinMaxQueue<I, S> {
    pub(crate) fn with_capacity(capacity: usize, mode: MinMaxMode) -> Self {
        Self {
            direct_queue: Vec::with_capacity(capacity),
            scored_queue: MinMaxHeap::with_capacity(capacity),
            mode,
            next_max: matches!(mode, MinMaxMode::Max | MinMaxMode::MinMax),
        }
    }
}

impl<I: Ord, S: Ord> ExplorationQueue<I, S> for MinMaxQueue<I, S> {
    fn push_scored(&mut self, item: I, score: S) {
        self.scored_queue.push(ScoredItem::new(item, score))
    }

    fn push(&mut self, item: I) {
        self.direct_queue.push(item);
    }

    fn pop(&mut self) -> Option<I> {
        self.direct_queue.pop().or_else(|| {
            let next = if self.next_max {
                self.scored_queue.pop_max()
            } else {
                self.scored_queue.pop_min()
            };
            if self.mode == MinMaxMode::MinMax {
                self.next_max = !self.next_max;
            }
            next.map(|s| s.item)
        })
    }
}
