# Legacy Cmd

## Description

This module aims to mimick the behaviour of the antique *centcore* daemon.

As for *centcore*, it reads a file (called command file) and process every commands that it knows of.

The module relies on the following modules to process commands:

* [Action](../core/action.md)
* [Proxy](../core/proxy.md)
* [Engine](engine.md)

## Configuration

| Directive                    | Description                                                  | Default value                             |
| :--------------------------- | :----------------------------------------------------------- | :---------------------------------------- |
| cmd_file                     | *Command file* to read commands from                         | `/var/lib/centreon/centcore.cmd`          |
| cmd_dir                      | Directory where to watch for *command files*                 | `/var/lib/centreon/`                      |
| cache_dir                    | Directory where to process Centreon configuration files      | `/var/cache/centreon/`                    |
| cache_dir_trap               | Directory where to process Centreontrapd databases           | `/etc/snmp/centreon_traps/`               |
| remote_dir                   | Directory where to export Remote Servers configuration       | `/var/cache/centreon/config/remote-data/` |
| bulk_external_cmd            | Bulk external commands (DOWNTIME, ACK,...)                   | `50`                                      |
| bulk_external_cmd_sequential | Order bulk external commands and other commands (Eg. RELOAD) | `1`                                       |
| legacycmd_result_timeout     | Max seconds to wait for a CENTREONCOMMAND batch's result     | `40`                                      |
| legacycmd_fast_poll_max_targets | Max combined pending targets for the 1s fast poll below (see `check_pending_batches_fast()`); above it, only the 5s sweep runs | `3` |

A CENTREONCOMMAND batch (SENDCFGFILE, SYNCTRAP, ENGINERESTART, RESTART, ENGINERELOAD,
RELOAD, START, STOP, RELOADBROKER, RESTARTCENTREONTRAPD, RELOADCENTREONTRAPD,
VMWARERESTART) reports one final log entry for its token once every dispatched
sub-action is done, or as soon as one fails: `data.legacycmd_final` is `1` and
`data.tracked` is `1`. Any other command (EXTERNALCMD, SENDEXPORTFILE, STARTWORKER)
closes its batch right after dispatch instead, with `data.tracked` at `0`: accepted,
but delivery is not confirmed.

A tracked batch's result surfaces through `check_pending_batches()`: every 5s, it nudges
each pending target with a `GETLOG` (so the proxy module fetches that node's result now
instead of waiting for its own ~20s `full_sync_history()` cycle, see
[proxy.md](../core/proxy.md)) and polls this central's own history for the batch's token.
That alone gave a `legacycmd_result_timeout`-bound latency of several multiples of 5s per
step even though the actual work is 1-2.5s (MON-214478). `check_pending_batches_fast()`
adds a 1s sweep on top: the local history poll (no shared resource involved) always runs,
giving much faster notice once a result lands by any path; the remote nudge only runs
there too while every pending batch's targets combined are at or under
`legacycmd_fast_poll_max_targets` - above that (a wide broadcast, e.g. a SENDCFGFILE to
every poller), the nudge stays on the unchanged 5s sweep, so a wide batch sends no more
httpserver-child traffic than before this change (see proxy.md's "httpserver child health
check" for why that traffic is bounded deliberately). The batch's deadline is only checked
on the 5s sweep, so a timeout can land up to ~4s late.

#### Example

```yaml
name: legacycmd
package: "gorgone::modules::centreon::legacycmd::hooks"
enable: true
cmd_file: "/var/lib/centreon/centcore.cmd"
cmd_dir: "/var/lib/centreon/"
cache_dir: "/var/cache/centreon/"
cache_dir_trap: "/etc/snmp/centreon_traps/"
remote_dir: "/var/cache/centreon/config/remote-data/"
```

## Events

| Event          | Description                       |
| :------------- | :-------------------------------- |
| LEGACYCMDREADY | Internal event to notify the core |

## API

No API endpoints.
