//! Command allowlist enforced by the agent.

use regex::RegexSet;

/// Error building an [`Allowlist`].
#[derive(Debug, thiserror::Error)]
#[error("invalid allowlist pattern: {0}")]
pub struct AllowlistError(#[from] regex::Error);

/// Set of regular expressions; a command is allowed when one of them matches
/// it entirely. An empty allowlist allows nothing.
#[derive(Debug, Clone)]
pub struct Allowlist {
    set: RegexSet,
}

impl Allowlist {
    /// Builds an allowlist. Each pattern is anchored on both ends.
    pub fn new<I, S>(patterns: I) -> Result<Self, AllowlistError>
    where
        I: IntoIterator<Item = S>,
        S: AsRef<str>,
    {
        let anchored = patterns
            .into_iter()
            .map(|p| format!("^(?:{})$", p.as_ref()));
        Ok(Self {
            set: RegexSet::new(anchored)?,
        })
    }

    /// True when `command` matches one pattern entirely.
    #[must_use]
    pub fn is_allowed(&self, command: &str) -> bool {
        self.set.is_match(command)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn full_match_only() {
        let list = Allowlist::new([r"/usr/lib/centreon/plugins/\S+( .*)?", "echo ok"]).unwrap();
        assert!(list.is_allowed("echo ok"));
        assert!(list.is_allowed("/usr/lib/centreon/plugins/check_x --mode=a"));
        assert!(!list.is_allowed("echo ok; rm -rf /"));
        assert!(!list.is_allowed("sh -c 'echo ok'"));
    }

    #[test]
    fn empty_allows_nothing() {
        let list = Allowlist::new(Vec::<String>::new()).unwrap();
        assert!(!list.is_allowed("true"));
    }
}
