//! Downtimes, comments and acknowledgements the engine keeps
//! (`downtimes/downtime_manager.cc`, `comment.cc`, the acknowledgement list
//! of `cbmod`). Records only: the rules that change them live in the core
//! (`core/downtimes.rs`), which owns the objects they refer to.

use std::collections::{BTreeMap, HashMap};

use serde::{Deserialize, Serialize};

/// `(host_id, service_id)`; `service_id` 0 for a host.
pub type Key = (u64, u64);

/// A scheduled downtime (`downtimes::downtime`), with what `cbmod` keeps
/// of it for its events (`cbmod::add_downtime`...).
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Downtime {
    pub id: u64,
    pub host_id: u64,
    /// 0 for a host downtime.
    pub service_id: u64,
    pub entry_time: i64,
    pub author: String,
    pub comment: String,
    pub start_time: i64,
    pub end_time: i64,
    pub fixed: bool,
    pub triggered_by: u64,
    /// Seconds.
    pub duration: u32,
    /// Started and not stopped yet.
    #[serde(skip)]
    pub in_effect: bool,
    /// A flexible downtime asked to start (`_start_flex_downtime`).
    #[serde(skip)]
    pub start_flex: bool,
    /// Counted in the pending flexible downtimes of its object.
    #[serde(skip)]
    pub incremented_pending: bool,
    /// Its comment (`_comment_id`).
    #[serde(skip)]
    pub comment_id: u64,
    /// Fields of the BBDO event: started, cancelled, actual times.
    #[serde(skip)]
    pub started: bool,
    #[serde(skip)]
    pub cancelled: bool,
    #[serde(skip)]
    pub actual_start: Option<i64>,
    #[serde(skip)]
    pub actual_end: Option<i64>,
}

impl Downtime {
    /// Key of its object.
    #[must_use]
    pub fn key(&self) -> Key {
        (self.host_id, self.service_id)
    }
}

/// Who made a comment (`comment::src`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Source {
    Internal,
    External,
}

/// Why a comment exists (`comment::e_type`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum EntryType {
    User,
    Downtime,
    Acknowledgement,
}

/// A comment (`comment`).
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Comment {
    pub id: u64,
    pub host_id: u64,
    /// 0 for a host comment.
    pub service_id: u64,
    pub entry_time: i64,
    pub author: String,
    pub data: String,
    pub persistent: bool,
    pub source: Source,
    pub entry_type: EntryType,
}

/// An acknowledgement as `cbmod` keeps it to close it later
/// (`cbmod::add_acknowledgement`).
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Acknowledgement {
    pub host_id: u64,
    pub service_id: u64,
    pub author: String,
    pub comment: String,
    pub entry_time: i64,
    pub sticky: bool,
    pub notify: bool,
    pub persistent: bool,
    /// State acknowledged.
    pub state: u32,
}

/// Every downtime, comment and open acknowledgement.
#[derive(Debug, Default)]
pub struct Book {
    pub downtimes: BTreeMap<u64, Downtime>,
    pub comments: BTreeMap<u64, Comment>,
    pub acknowledgements: HashMap<Key, Acknowledgement>,
    /// Last downtime id given (`downtime_manager::_next_id`).
    last_downtime_id: u64,
}

impl Book {
    /// `get_next_downtime_id`: one more than the highest id ever seen.
    pub fn next_downtime_id(&mut self) -> u64 {
        let highest = self.downtimes.keys().next_back().copied().unwrap_or(0);
        self.last_downtime_id = self.last_downtime_id.max(highest) + 1;
        self.last_downtime_id
    }

    /// Highest downtime id given.
    #[must_use]
    pub fn last_downtime_id(&self) -> u64 {
        self.last_downtime_id
    }

    /// Ids already given (retention): never given again.
    pub fn set_last_downtime_id(&mut self, id: u64) {
        self.last_downtime_id = self.last_downtime_id.max(id);
    }

    /// `comment::comment`: the smallest id not in use.
    #[must_use]
    pub fn next_comment_id(&self) -> u64 {
        (1..)
            .find(|id| !self.comments.contains_key(id))
            .unwrap_or(1)
    }

    /// Downtimes of an object.
    pub fn downtimes_of(&self, key: Key) -> impl Iterator<Item = &Downtime> {
        self.downtimes.values().filter(move |d| d.key() == key)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ids_as_engine() {
        let mut book = Book::default();
        assert_eq!(book.next_downtime_id(), 1);
        assert_eq!(book.next_downtime_id(), 2);
        let comment = Comment {
            id: 1,
            host_id: 1,
            service_id: 0,
            entry_time: 0,
            author: String::new(),
            data: String::new(),
            persistent: false,
            source: Source::External,
            entry_type: EntryType::User,
        };
        book.comments.insert(1, comment.clone());
        book.comments.insert(3, Comment { id: 3, ..comment });
        // Comment ids are reused, downtime ids never.
        assert_eq!(book.next_comment_id(), 2);
        book.comments.remove(&1);
        assert_eq!(book.next_comment_id(), 1);
        assert_eq!(book.next_downtime_id(), 3);
    }
}
