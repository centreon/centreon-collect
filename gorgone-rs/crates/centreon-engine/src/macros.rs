//! Macro expansion of check command lines, as Engine does it
//! (`engine/src/macros/process.cc`, `engine/src/utils.cc`), reduced to the
//! macros listed in the design note.

use std::collections::{BTreeMap, HashMap};

use crate::crypto::Credentials;

/// Engine's `MAX_COMMAND_ARGUMENTS`.
const MAX_ARGS: usize = 32;

/// State of an object as the `$…STATE…$` macros show it.
#[derive(Debug, Clone, Default)]
pub struct StateView {
    /// `UP`, `DOWN`, `OK`, `WARNING`…
    pub state: &'static str,
    /// Numeric state.
    pub state_id: u32,
    /// `SOFT` or `HARD`.
    pub state_type: &'static str,
    /// Current attempt.
    pub attempt: u32,
}

/// The host side of a macro context.
#[derive(Debug, Clone, Default)]
pub struct HostView<'a> {
    /// `host_name`.
    pub name: &'a str,
    /// `alias`.
    pub alias: &'a str,
    /// `address`.
    pub address: &'a str,
    /// Custom macros, names upper-cased.
    pub macros: Option<&'a BTreeMap<String, String>>,
    /// Current state.
    pub state: StateView,
}

/// The service side of a macro context.
#[derive(Debug, Clone, Default)]
pub struct ServiceView<'a> {
    /// `service_description`.
    pub description: &'a str,
    /// Custom macros, names upper-cased.
    pub macros: Option<&'a BTreeMap<String, String>>,
    /// Current state.
    pub state: StateView,
}

/// What macros resolve against for one check.
pub struct Context<'a> {
    /// Resource macros (`USER1`…), without dollars.
    pub users: &'a HashMap<String, String>,
    /// The host checked, or the host of the service checked.
    pub host: HostView<'a>,
    /// The service checked, if any.
    pub service: Option<ServiceView<'a>>,
    /// `$ARGn$` values, already expanded.
    args: Vec<String>,
    /// Keys of `encrypt::` values: with them, values are read as Engine
    /// reads them (`macros/grab_value.cc`), `raw::` stripped.
    secrets: Option<&'a Credentials>,
}

impl<'a> Context<'a> {
    /// A context without arguments.
    #[must_use]
    pub fn new(
        users: &'a HashMap<String, String>,
        host: HostView<'a>,
        service: Option<ServiceView<'a>>,
    ) -> Self {
        Self {
            users,
            host,
            service,
            args: Vec::new(),
            secrets: None,
        }
    }

    /// Reads `encrypt::` and `raw::` values with `credentials`.
    #[must_use]
    pub fn with_secrets(mut self, credentials: Option<&'a Credentials>) -> Self {
        self.secrets = credentials;
        self
    }

    /// A macro value once its `encrypt::`/`raw::` prefix is handled;
    /// `None` when it has none or there are no keys. A value that does
    /// not decrypt stays as it is, as in Engine; the error names the macro
    /// only.
    fn reveal(&self, name: &str, value: &str) -> Option<String> {
        match self.secrets?.reveal(value) {
            Ok(revealed) => revealed,
            Err(error) => {
                tracing::error!(
                    macro_name = name,
                    "fail to decrypt content of macro: {error}"
                );
                None
            }
        }
    }

    /// Value of macro `name`, `None` when unknown.
    fn grab(&self, name: &str, depth: u32) -> Option<String> {
        let value = self.grab_raw(name, depth)?;
        Some(self.reveal(name, &value).unwrap_or(value))
    }

    /// Value of macro `name` before its prefix is handled.
    fn grab_raw(&self, name: &str, depth: u32) -> Option<String> {
        let host = &self.host;
        let service = self.service.as_ref();
        let value = match name {
            "HOSTNAME" | "HOSTDISPLAYNAME" => host.name.to_owned(),
            "HOSTALIAS" => host.alias.to_owned(),
            "HOSTADDRESS" => host.address.to_owned(),
            "HOSTSTATE" => host.state.state.to_owned(),
            "HOSTSTATEID" => host.state.state_id.to_string(),
            "HOSTSTATETYPE" => host.state.state_type.to_owned(),
            "HOSTATTEMPT" => host.state.attempt.to_string(),
            "SERVICEDESC" | "SERVICEDISPLAYNAME" => service?.description.to_owned(),
            "SERVICESTATE" => service?.state.state.to_owned(),
            "SERVICESTATEID" => service?.state.state_id.to_string(),
            "SERVICESTATETYPE" => service?.state.state_type.to_owned(),
            "SERVICEATTEMPT" => service?.state.attempt.to_string(),
            _ => {
                if let Some(index) = name.strip_prefix("ARG") {
                    let index: usize = index.parse().ok()?;
                    if index == 0 || index > MAX_ARGS {
                        return None;
                    }
                    return Some(self.args.get(index - 1).cloned().unwrap_or_default());
                }
                let custom = if let Some(key) = name.strip_prefix("_HOST") {
                    host.macros.and_then(|macros| macros.get(key))
                } else if let Some(key) = name.strip_prefix("_SERVICE") {
                    service
                        .and_then(|service| service.macros)
                        .and_then(|macros| macros.get(key))
                } else {
                    None
                };
                if let Some(value) = custom {
                    // A secret is used as it is; other values are expanded
                    // in turn.
                    if let Some(revealed) = self.reveal(name, value) {
                        return Some(revealed);
                    }
                    return Some(self.expand_at(value, depth + 1));
                }
                return self.users.get(name).cloned();
            }
        };
        Some(value)
    }

    /// Expands `input` at recursion `depth`: an unknown macro disappears at
    /// depth 0 and stays as written deeper, as in Engine.
    fn expand_at(&self, input: &str, depth: u32) -> String {
        // Bounded: a macro naming itself must not recurse forever.
        if depth > 8 {
            return input.to_owned();
        }
        let mut output = String::with_capacity(input.len());
        let mut rest = input;
        while let Some(begin) = rest.find('$') {
            output.push_str(&rest[..begin]);
            let after = &rest[begin + 1..];
            if let Some(escaped) = after.strip_prefix('$') {
                output.push('$');
                rest = escaped;
                continue;
            }
            let Some(end) = after.find('$') else {
                // No closing dollar: the rest is kept as it is.
                output.push_str(&rest[begin..]);
                return output;
            };
            let name = &after[..end];
            match self.grab(name, depth) {
                Some(value) => output.push_str(&value),
                None if depth > 0 => {
                    output.push('$');
                    output.push_str(name);
                    output.push('$');
                }
                None => {}
            }
            rest = &after[end + 1..];
        }
        output.push_str(rest);
        output
    }

    /// Expands the macros of `input`.
    #[must_use]
    pub fn expand(&self, input: &str) -> String {
        self.expand_at(input, 0)
    }

    /// Command line of `check_command` (`name!arg1!arg2`), whose command
    /// line is `command_line`, with every macro expanded: the arguments
    /// first, each in turn, then the line.
    #[must_use]
    pub fn check_command_line(&mut self, check_command: &str, command_line: &str) -> String {
        self.args.clear();
        if let Some((_, raw_args)) = check_command.split_once('!') {
            let mut chars = raw_args.chars();
            let mut current = String::new();
            let mut raw = Vec::new();
            loop {
                match chars.next() {
                    Some('\\') => {
                        if let Some(next) = chars.next() {
                            current.push(next);
                        }
                    }
                    Some('!') => raw.push(std::mem::take(&mut current)),
                    Some(c) => current.push(c),
                    None => {
                        raw.push(current);
                        break;
                    }
                }
            }
            for arg in raw.into_iter().take(MAX_ARGS) {
                let expanded = self.expand(&arg);
                self.args.push(expanded);
            }
        }
        self.expand(command_line)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn host_macros() -> BTreeMap<String, String> {
        BTreeMap::from([
            ("SNMPCOMMUNITY".to_owned(), "public".to_owned()),
            // The content of a custom macro is expanded too.
            ("URL".to_owned(), "http://$HOSTADDRESS$/$NOPE$".to_owned()),
        ])
    }

    #[test]
    fn expands_engine_macros() {
        let users = HashMap::from([
            ("USER1".to_owned(), "/usr/lib/nagios/plugins".to_owned()),
            (
                "CENTREONPLUGINS".to_owned(),
                "/usr/lib/centreon/plugins".to_owned(),
            ),
        ]);
        let hmacros = host_macros();
        let smacros = BTreeMap::from([("CRIT".to_owned(), "90".to_owned())]);
        let mut ctx = Context::new(
            &users,
            HostView {
                name: "h1",
                alias: "First",
                address: "10.0.0.1",
                macros: Some(&hmacros),
                state: StateView {
                    state: "UP",
                    state_id: 0,
                    state_type: "HARD",
                    attempt: 1,
                },
            },
            Some(ServiceView {
                description: "Disk /",
                macros: Some(&smacros),
                state: StateView {
                    state: "WARNING",
                    state_id: 1,
                    state_type: "SOFT",
                    attempt: 2,
                },
            }),
        );
        let line = ctx.check_command_line(
            "cmd!$_HOSTSNMPCOMMUNITY$!a\\!b!$_SERVICECRIT$",
            "$USER1$/check -H $HOSTADDRESS$ -C $ARG1$ -x '$ARG2$' -c $ARG3$ $ARG4$ \
             -n $HOSTNAME$/$HOSTALIAS$/$SERVICEDESC$ $SERVICESTATE$ $SERVICESTATEID$ \
             $SERVICESTATETYPE$ $SERVICEATTEMPT$ $HOSTSTATE$ $$5 $UNKNOWN$x $CENTREONPLUGINS$",
        );
        assert_eq!(
            line,
            "/usr/lib/nagios/plugins/check -H 10.0.0.1 -C public -x 'a!b' -c 90  \
             -n h1/First/Disk / WARNING 1 SOFT 2 UP $5 x /usr/lib/centreon/plugins"
        );
        assert_eq!(ctx.expand("$_HOSTURL$"), "http://10.0.0.1/$NOPE$");
        assert_eq!(ctx.expand("no end $HOSTNAME"), "no end $HOSTNAME");
    }

    #[test]
    fn secrets_are_decrypted_at_resolution() {
        use crate::crypto::tests::{BENCH_KEY, FROM_WEB};
        let keys = Credentials::from_json(BENCH_KEY.as_bytes()).unwrap();
        let users = HashMap::from([
            ("USER2".to_owned(), format!("encrypt::{}", FROM_WEB.trim())),
            ("USER3".to_owned(), "raw::$dollar".to_owned()),
        ]);
        let hmacros = BTreeMap::from([
            ("SECRET".to_owned(), format!("encrypt::{}", FROM_WEB.trim())),
            (
                "BROKEN".to_owned(),
                "encrypt::bm90IGVuY3J5cHRlZA==".to_owned(),
            ),
        ]);
        let host = HostView {
            name: "h",
            macros: Some(&hmacros),
            ..HostView::default()
        };
        let mut ctx = Context::new(&users, host.clone(), None).with_secrets(Some(&keys));
        let line = ctx.check_command_line("cmd!$_HOSTSECRET$", "/p $ARG1$ $USER2$ $USER3$");
        assert_eq!(line, "/p s3cr3t-value s3cr3t-value $dollar");
        // Not decryptable: kept as written.
        assert!(ctx.expand("$_HOSTBROKEN$").starts_with("encrypt::"));
        // Without keys (or a version 1 document), values are literal.
        let ctx = Context::new(&users, host, None);
        assert!(ctx.expand("$_HOSTSECRET$").starts_with("encrypt::"));
        assert_eq!(ctx.expand("$USER3$"), "raw::$dollar");
    }
}
