# Centreon Perl Connector

The Perl connector executes Nagios/Centreon supervision scripts written in Perl without restarting
the interpreter on every check. It sits between Centreon Engine and the `.pl` scripts using the
standard connector protocol (stdin/stdout).

## Process hierarchy

```
Centreon Engine   (text protocol, NUL-delimited commands)
      │
      ▼
   policy          — main connector process
      │  protobuf over pipes (ConnectorMess)
      ▼
 script_child      — one process per unique .pl script path
      │  protobuf over pipes (ConnectorMess)
      ▼
 check_child       — pool of workers inside script_child
```

### policy

The main process. It receives execute requests from Engine, routes them to the `script_child`
that owns the requested script (created on first use), and returns results to Engine.

It maintains:

- `_scripts`: active `script_child` instances, indexed by script path.
- `_check_child_stats`: memory/fd/thread footprint of every `check_child`, used for memory
  pressure management.
- `_pending_queries`: in-flight requests, indexed by `cmd_id`, timeout and `script_child`.

A one-second timer expires requests that have exceeded their timeout and reaps dying
`script_child` processes once all their pending queries have completed.

### script_child

A process forked by `policy`, one per unique `.pl` script path. It owns an embedded Perl
interpreter (`PerlInterpreter`) and compiles the target script once via
`Embed::Persistent::eval_file`, wrapping it into a reusable Perl subroutine.

Inside the forked process, an asio event loop receives execute requests from `policy` and
dispatches them to idle `check_child` workers. If all workers are busy and creation is allowed
(see `--max-child` and `--min-free-memory`), a new `check_child` is forked. Otherwise the
request is queued, ordered by deadline.

A one-second timer watches the script file's mtime. If it changes, a `have_to_terminate`
message is sent to `policy`, which removes this `script_child` from its active table and lets
it die gracefully once its in-flight queries complete — then creates a fresh `script_child` that
will recompile the updated version.

### check_child

A process forked by `script_child`, one per concurrency slot. It executes the compiled Perl
subroutine synchronously, captures stdout and stderr through internal pipes, and sends the result
(status, stdout, stderr, resource metrics) back to `script_child` via the protobuf protocol.

The embedded Perl interpreter overrides `CORE::GLOBAL::exit` so that `exit($code)` calls from
the script write `SCRIPT_EXIT_CODE:$code\n` to stderr before raising a Perl exception.
`check_child` parses this marker to recover the exit code. The remaining stderr content is
forwarded as-is in the `stderr` field of the result.

After each execution, `check_child` measures its own resource footprint (resident memory, open
file descriptors, thread count) and includes it in the result. `script_child` compares the
first-check and last-check values to decide whether the process should be recycled.

## Global parameters

These parameters are passed on the connector command line (configured in Engine via
`connector_line`).

| Parameter | Default | Description |
|---|---|---|
| `--max-child N` | 64 | Maximum total number of child processes (`script_child` + `check_child` combined). When the ceiling is reached, new requests are queued without forking an additional `check_child`. |
| `--min-free-memory N` | 500 | Minimum free system memory in MB. Below this threshold no new `check_child` is created, and the heaviest idle `check_child` is killed to try to reclaim memory. |
| `--child-max-reuse-script N` | 100 | Maximum number of checks a single `check_child` may execute. Once reached, the process is killed after the current check and replaced if needed. Limits memory leaks from poorly written scripts. |
| `--child-max-memory-increase-percent N` | 10 | Maximum allowed growth in resident memory between the first and the last check executed by a `check_child`. Exceeding this threshold causes the process to be killed. |
| `--child-max-fd-increase-percent N` | 10 | Maximum allowed growth in the number of open file descriptors between first and last check. |
| `--child-max-thread N` | 10 | Maximum number of threads a `check_child` may create. If the thread count exceeds this value after a check, the process is killed. |
| `--idle-child-ttl N` | 15 | Maximum idle time for a `check_child` in minutes. A process that has executed no check for this duration is killed. |
| `--log-file PATH` | stderr | Log file path. |
| `--log-level LEVEL` | info | Log verbosity: `error`, `info`, `debug`, `trace`. |
| `--code CODE` | — | Extra Perl code executed by the embedded interpreter at `script_child` startup. |

## Per-command overrides

Some thresholds can be adjusted on a per-command basis by appending key-value pairs to the check
command line (after the script path). These values override the global configuration for that
single check only.

| Key | Equivalent parameter |
|---|---|
| `child-max-reuse-script N` | `--child-max-reuse-script` |
| `child-max-memory-increase-percent N` | `--child-max-memory-increase-percent` |
| `child-max-fd-increase-percent N` | `--child-max-fd-increase-percent` |
| `child-max-thread N` | `--child-max-thread` |

Example in a Centreon command definition:

```
/usr/lib/centreon/plugins/check_something.pl --host $HOSTADDRESS$ child-max-reuse-script 10
```

## Memory pressure management

When `policy` is about to create a new `check_child` (on receipt of an execute request), it
evaluates available system memory via `/proc/meminfo`:

1. If free memory is below `--min-free-memory`, `no_child_create = true` is set in the `Execute`
   message sent to `script_child`. The `script_child` queues the request instead of forking.

2. In parallel, `policy` calls `_remove_heaviest_check_child()`, which looks for the
   heaviest `check_child` across all tracked processes using a two-round strategy:
   - **Round 0**: looks for an idle `check_child` (no check currently running) that belongs to a
     `script_child` owning at least two workers. If found, it receives a `Terminate` with
     `immediate = true` (killed immediately).
   - **Round 1**: if all workers are busy, selects the heaviest one and sends a `Terminate` with
     `immediate = false` (deferred stop, after the current check finishes).

3. If the total process count (`_scripts.size() + _check_child_stats.size()`) reaches
   `--max-child`, `_remove_oldest_check_child()` kills the least recently used `check_child`.

## Live script reload

`script_child` compares the script file's mtime against the value recorded at compile time every
second. If the file has been modified:

1. It sends a `have_to_terminate` to `policy` indicating the update.
2. `policy` moves the `script_child` to the `_dying_scripts` set and stops sending it new
   requests.
3. Once all pending queries for that script have completed, `policy` calls `kill()` on the dying
   `script_child`.
4. The next request for the same script path creates a fresh `script_child` that recompiles the
   updated file.

The reload is transparent to Engine: in-flight checks complete normally, subsequent checks use
the new script.

## Communication protocols

### Engine ↔ policy (text)

Commands delimited by 4 NUL bytes. Command identifiers:

| ID | Command |
|---|---|
| 0 | Version query |
| 2 | Execute |
| 4 | Quit |
| 10 | EOF (no-more-stdin) |

### policy ↔ script_child and script_child ↔ check_child (binary)

Length-prefixed protobuf `ConnectorMess` messages over POSIX pipes. The schema is defined in
`connectors/perl/src/perl_connector.proto`.

Message types:

| Message | Direction | Description |
|---|---|---|
| `Execute` | → child | Orders the execution of a check |
| `Result` | ← child | Check result (status, stdout, stderr, resource metrics) |
| `Terminate` | → child | Shutdown request (immediate or deferred) |
| `CheckChildEnd` | script_child → policy | Notification that a `check_child` has exited |
| `GlobalError` (`have_to_terminate`) | script_child → policy | Fatal error or reload required |
