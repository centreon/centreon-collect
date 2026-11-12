//! Command allowlist enforced by the agent.

use std::path::{Component, Path};

use regex::RegexSet;

/// Error building an [`Allowlist`].
#[derive(Debug, thiserror::Error)]
#[error("invalid allowlist pattern: {0}")]
pub struct AllowlistError(#[from] regex::Error);

/// Why a command was refused.
#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum Denied {
    /// No pattern matches the command.
    #[error("command does not match the allowlist")]
    NotListed,
    /// Control character (newline, NUL...) in the command.
    #[error("command contains a control character")]
    ControlCharacter,
    /// Unbalanced quotes or empty command.
    #[error("command cannot be parsed")]
    Unparsable,
    /// Program given with a relative path, or with `.` / `..` components.
    #[error("program path must be absolute, without '.' or '..'")]
    ProgramPath,
}

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

    /// True when `command` matches one pattern entirely. Only checks the
    /// text: use [`Allowlist::authorize`] before running without a shell.
    #[must_use]
    pub fn is_allowed(&self, command: &str) -> bool {
        self.set.is_match(command)
    }

    /// Checks `command` and returns the argument vector to execute.
    ///
    /// On top of the pattern match, the program must be what the pattern
    /// reader expects: a path with a `/` has to be absolute and free of `.`
    /// and `..`, so `/allowed/dir/../../bin/sh` cannot pass a
    /// `/allowed/dir/\S+` pattern.
    pub fn authorize(&self, command: &str) -> Result<Vec<String>, Denied> {
        if command.chars().any(char::is_control) {
            return Err(Denied::ControlCharacter);
        }
        if !self.is_allowed(command) {
            return Err(Denied::NotListed);
        }
        let argv = shell_words::split(command).map_err(|_| Denied::Unparsable)?;
        let program = argv.first().ok_or(Denied::Unparsable)?;
        if program.contains('/') {
            let path = Path::new(program);
            let plain = path
                .components()
                .all(|c| matches!(c, Component::RootDir | Component::Normal(_)));
            // `components()` hides `/./` and trailing `/.`, so look at the text too.
            if !path.is_absolute() || !plain || program.split('/').any(|part| part == ".") {
                return Err(Denied::ProgramPath);
            }
        }
        Ok(argv)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn list() -> Allowlist {
        Allowlist::new([r"/usr/lib/centreon/plugins/\S+( .*)?", "echo ok"]).unwrap()
    }

    #[test]
    fn full_match_only() {
        let list = list();
        assert!(list.is_allowed("echo ok"));
        assert!(list.is_allowed("/usr/lib/centreon/plugins/check_x --mode=a"));
        assert!(!list.is_allowed("echo ok; rm -rf /"));
        assert!(!list.is_allowed("sh -c 'echo ok'"));
    }

    #[test]
    fn empty_allows_nothing() {
        let list = Allowlist::new(Vec::<String>::new()).unwrap();
        assert!(!list.is_allowed("true"));
        assert_eq!(list.authorize("true"), Err(Denied::NotListed));
    }

    #[test]
    fn authorize_returns_argv() {
        assert_eq!(
            list().authorize("/usr/lib/centreon/plugins/check_x --name 'a b'"),
            Ok(vec![
                "/usr/lib/centreon/plugins/check_x".to_owned(),
                "--name".to_owned(),
                "a b".to_owned()
            ])
        );
        assert_eq!(
            list().authorize("echo ok"),
            Ok(vec!["echo".into(), "ok".into()])
        );
    }

    #[test]
    fn authorize_refuses_escapes() {
        let list = list();
        for command in [
            "/usr/lib/centreon/plugins/../../../../bin/sh -c id",
            "/usr/lib/centreon/plugins/./check_x",
            "/usr/lib/centreon/plugins/check_x/..",
        ] {
            assert_eq!(
                list.authorize(command),
                Err(Denied::ProgramPath),
                "{command}"
            );
        }
        assert_eq!(
            list.authorize("/usr/lib/centreon/plugins/check_x a\nb"),
            Err(Denied::ControlCharacter)
        );
        assert_eq!(
            list.authorize("/usr/lib/centreon/plugins/check_x 'a"),
            Err(Denied::Unparsable)
        );
        let relative = Allowlist::new([r"plugins/\S+"]).unwrap();
        assert_eq!(
            relative.authorize("plugins/check_x"),
            Err(Denied::ProgramPath)
        );
    }
}
