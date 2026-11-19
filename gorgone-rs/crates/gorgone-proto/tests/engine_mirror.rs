//! `proto/com/centreon/engine/engine.proto` copies the few declarations the
//! agent uses from the real Engine API. This test fails when the two diverge,
//! so a change of the Engine contract cannot go unnoticed here.

use std::path::Path;

/// Comments removed, whitespace collapsed.
fn normalise(source: &str) -> String {
    let mut without_blocks = String::with_capacity(source.len());
    let mut rest = source;
    while let Some(start) = rest.find("/*") {
        without_blocks.push_str(&rest[..start]);
        rest = rest[start..]
            .find("*/")
            .map_or("", |end| &rest[start + end + 2..]);
    }
    without_blocks.push_str(rest);
    without_blocks
        .lines()
        .map(|line| line.split("//").next().unwrap_or_default())
        .flat_map(str::split_whitespace)
        .collect::<Vec<_>>()
        .join(" ")
}

/// Every `rpc …` statement and every top-level `message … { … }` block.
fn declarations(proto: &str) -> Vec<String> {
    let mut found = Vec::new();
    let mut rest = proto;
    while let Some(start) = rest.find("rpc ") {
        let end = start + rest[start..].find(')').unwrap();
        let end = end + 1 + rest[end + 1..].find(')').unwrap() + 1;
        found.push(rest[start..end].to_owned());
        rest = &rest[end..];
    }
    let mut depth = 0usize;
    let mut block_start = None;
    for (index, byte) in proto.bytes().enumerate() {
        match byte {
            b'{' => depth += 1,
            b'}' => {
                depth -= 1;
                if let (0, Some(start)) = (depth, block_start.take()) {
                    found.push(proto[start..=index].to_owned());
                }
            }
            _ if depth == 0 && block_start.is_none() && proto[index..].starts_with("message ") => {
                block_start = Some(index);
            }
            _ => {}
        }
    }
    found
}

#[test]
fn mirror_matches_the_engine_api() {
    let crate_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
    let real = crate_dir.join("../../../engine/enginerpc/engine.proto");
    // The workspace can be built outside the centreon-collect repository.
    let Ok(real) = std::fs::read_to_string(real) else {
        eprintln!("engine.proto not found, mirror not checked");
        return;
    };
    let real = normalise(&real);
    let mirror =
        std::fs::read_to_string(crate_dir.join("../../proto/com/centreon/engine/engine.proto"))
            .unwrap();
    let declarations = declarations(&normalise(&mirror));
    assert!(declarations.len() >= 6, "{declarations:?}");
    for declaration in declarations {
        assert!(
            real.contains(&declaration),
            "not in engine/enginerpc/engine.proto as declared here: {declaration}"
        );
    }
}
