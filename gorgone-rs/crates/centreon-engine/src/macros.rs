//! Macro expansion of check command lines, as Engine does it
//! (`engine/src/macros/process.cc`, `engine/src/utils.cc`), reduced to the
//! macros listed in the design note.

use std::collections::{BTreeMap, HashMap};

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
        }
    }

    /// Value of macro `name`, `None` when unknown.
    fn grab(&self, name: &str, depth: u32) -> Option<String> {
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
                    // The content of a custom macro is expanded in turn.
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
}
