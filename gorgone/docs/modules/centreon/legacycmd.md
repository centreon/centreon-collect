# Legacy Cmd

## Description

This module aims to mimick the behaviour of the antique *centcore* daemon.

As for *centcore*, it reads a file (called command file) and process every commands that it knows of.

The module relies on the following modules to process commands:

* [Action](../core/action.md)
* [Proxy](../core/proxy.md)
* [Engine](engine.md)

## Configuration

| Directive | Description | Default value |
| :- | :- | :- |
| cmd_file | *Command file* to read commands from | `/var/lib/centreon/centcore.cmd` |
| cmd_dir | Directory where to watch for *command files* | `/var/lib/centreon/` |
| cache_dir | Directory where to process Centreon configuration files | `/var/cache/centreon/` |
| cache_dir_trap | Directory where to process Centreontrapd databases | `/etc/snmp/centreon_traps/` |
| remote_dir | Directory where to export Remote Servers configuration | `/var/cache/centreon/config/remote-data/` |
| clapi_user | Centreon user with admin rights used to execute CLAPI commands | `admin` |

CLAPI user, and CLAPI user's password, must be the same on both Central and
Remote Servers if platform uses those kind of servers.

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
clapi_user: "admin"
```

## Events

| Event | Description |
| :- | :- |
| LEGACYCMDREADY | Internal event to notify the core |

## API

No API endpoints.
