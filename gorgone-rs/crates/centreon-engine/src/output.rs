//! Plugin output parsing, as `common::parse_check_output`
//! (`common/src/perfdata.cc`) with the options Engine passes for checks:
//! long output lines joined by a literal `\n`, real new lines in the input.

/// Output of a check split into its three parts.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct CheckOutput {
    /// First line, before its last `|`.
    pub output: String,
    /// Following lines, joined by `\n` (two characters).
    pub long_output: String,
    /// Performance data.
    pub perfdata: String,
}

/// `isspace` of the C locale.
fn is_space(byte: u8) -> bool {
    matches!(byte, b' ' | b'\t' | b'\n' | b'\x0b' | b'\x0c' | b'\r')
}

/// Splits `buffer` as Engine does, quirks included: the first line gives the
/// output and the perfdata after its last `|`; the next lines are long
/// output until one holds a `|`, after which every line is perfdata.
#[must_use]
pub fn parse(buffer: &str) -> CheckOutput {
    let mut parsed = CheckOutput::default();
    let mut long_pipe = false;
    let mut perfdata_filled = false;
    for (index, line) in buffer.split('\n').enumerate() {
        let number = index + 1;
        let bytes = line.as_bytes();
        let pipe = if long_pipe {
            None
        } else {
            bytes.iter().rposition(|byte| *byte == b'|')
        };
        match pipe {
            Some(pipe) => {
                let mut end = pipe;
                while end > 1 && is_space(bytes[end - 1]) {
                    end -= 1;
                }
                let mut start = pipe + 1;
                while start + 1 < bytes.len() && is_space(bytes[start]) {
                    start += 1;
                }
                let start = start.min(bytes.len());
                if number == 1 {
                    parsed.output.push_str(&line[..end]);
                    parsed.perfdata.push_str(&line[start..]);
                    perfdata_filled = true;
                } else {
                    if number > 2 {
                        parsed.long_output.push_str("\\n");
                    }
                    parsed.long_output.push_str(&line[..end]);
                    if perfdata_filled {
                        parsed.perfdata.push(' ');
                    }
                    parsed.perfdata.push_str(&line[start..]);
                    long_pipe = true;
                }
            }
            None => {
                let mut end = bytes.len();
                while end > 1 && is_space(bytes[end - 1]) {
                    end -= 1;
                }
                let line = &line[..end];
                if number == 1 {
                    parsed.output.push_str(line);
                } else if !long_pipe {
                    if number > 2 {
                        parsed.long_output.push_str("\\n");
                    }
                    parsed.long_output.push_str(line);
                } else {
                    if perfdata_filled {
                        parsed.perfdata.push(' ');
                    }
                    parsed.perfdata.push_str(line);
                }
            }
        }
    }
    parsed
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parts(text: &str) -> (String, String, String) {
        let parsed = parse(text);
        (parsed.output, parsed.long_output, parsed.perfdata)
    }

    #[test]
    fn single_line() {
        assert_eq!(
            parts("OK - fine | a=1;2;3 b=4\n"),
            ("OK - fine".into(), String::new(), "a=1;2;3 b=4".into())
        );
        assert_eq!(parts("OK"), ("OK".into(), String::new(), String::new()));
        // The last pipe separates.
        assert_eq!(parts("a|b|c=1").0, "a|b");
    }

    #[test]
    fn long_output_then_perfdata() {
        let text = "WARN - x | a=1\nline 2\nline 3 | b=2\nc=3\n";
        assert_eq!(
            parts(text),
            (
                "WARN - x".into(),
                "line 2\\nline 3".into(),
                // The empty line after the last new line adds a blank, as
                // in Engine.
                "a=1 b=2 c=3 ".into()
            )
        );
        // Without perfdata on the first line, no leading blank is added.
        assert_eq!(
            parts("OK\nmore | x=1\ny=2"),
            ("OK".into(), "more".into(), "x=1y=2".into())
        );
    }
}
