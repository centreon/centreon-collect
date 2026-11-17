//! Splitting of a processed command line into a program and its arguments,
//! as Engine does it (`common/process/src/process_args.cc`): no shell.

/// Why a command line cannot be run.
#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum SplitError {
    /// A quote was opened and never closed.
    #[error("missing separator '{0}'")]
    UnclosedQuote(char),
    /// Nothing but blanks.
    #[error("empty command line")]
    Empty,
}

/// Splits `line`: blanks separate arguments, `'…'` and `"…"` quote (also
/// inside a word), `\` escapes the next character and `\n \r \t \a \b \v \f`
/// give the control character.
pub fn split(line: &str) -> Result<Vec<String>, SplitError> {
    #[derive(PartialEq)]
    enum State {
        WaitingBegin,
        Field,
        Quoted(char),
    }
    let mut args: Vec<String> = Vec::new();
    let mut current = String::new();
    let mut state = State::WaitingBegin;
    let mut escaped = false;
    let escape = |c: char| match c {
        'n' => '\n',
        'r' => '\r',
        't' => '\t',
        'a' => '\u{7}',
        'b' => '\u{8}',
        'v' => '\u{b}',
        'f' => '\u{c}',
        other => other,
    };
    for c in line.chars() {
        if escaped {
            current.push(escape(c));
            escaped = false;
            if state == State::WaitingBegin {
                state = State::Field;
            }
            continue;
        }
        match state {
            State::WaitingBegin => match c {
                '\\' => escaped = true,
                '"' | '\'' => state = State::Quoted(c),
                c if c.is_ascii_whitespace() => {}
                c => {
                    state = State::Field;
                    current.push(c);
                }
            },
            State::Field => match c {
                '\\' => escaped = true,
                c if c.is_ascii_whitespace() => {
                    args.push(std::mem::take(&mut current));
                    state = State::WaitingBegin;
                }
                '"' | '\'' => state = State::Quoted(c),
                c => current.push(c),
            },
            State::Quoted(quote) => match c {
                '\\' => escaped = true,
                c if c == quote => state = State::Field,
                c => current.push(c),
            },
        }
    }
    if let State::Quoted(quote) = state {
        return Err(SplitError::UnclosedQuote(quote));
    }
    if state == State::Field {
        args.push(current);
    }
    // Engine takes the first field as the program, even an empty one.
    if args.is_empty() || args[0].is_empty() {
        return Err(SplitError::Empty);
    }
    Ok(args)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ok(line: &str) -> Vec<String> {
        split(line).unwrap()
    }

    #[test]
    fn splits_like_engine() {
        assert_eq!(ok("/bin/echo a  b\tc"), ["/bin/echo", "a", "b", "c"]);
        assert_eq!(
            ok(r#"/check -w "1 2" -c 'x y'"#),
            ["/check", "-w", "1 2", "-c", "x y"]
        );
        // A quote inside a word joins the quoted part to it.
        assert_eq!(ok(r#"/p --a="b c"d"#), ["/p", "--a=b cd"]);
        // Escapes, also inside quotes and at the start of a field.
        assert_eq!(ok(r"/p a\ b \n x"), ["/p", "a b", "\n", "x"]);
        assert_eq!(ok(r#"/p "a\"b" 'c\'d'"#), ["/p", "a\"b", "c'd"]);
        assert_eq!(ok(r"/p \t"), ["/p", "\t"]);
        // An empty quoted argument is an argument.
        assert_eq!(ok(r#"/p "" x"#), ["/p", "", "x"]);
        assert_eq!(ok("  /p  "), ["/p"]);
        // No shell: these are plain arguments.
        assert_eq!(ok("/p a|b;c $X"), ["/p", "a|b;c", "$X"]);
    }

    #[test]
    fn refuses_what_engine_refuses() {
        assert_eq!(split("/p 'a"), Err(SplitError::UnclosedQuote('\'')));
        assert_eq!(split("/p \"a"), Err(SplitError::UnclosedQuote('"')));
        assert_eq!(split("   "), Err(SplitError::Empty));
        assert_eq!(split(""), Err(SplitError::Empty));
    }
}
