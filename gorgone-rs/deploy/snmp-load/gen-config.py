#!/usr/bin/env python3
"""Writes the Engine files of a load run: SERVICES services on hosts of
SERVICES_PER_HOST services each, every host bound to one SNMP agent address
(round robin over the addresses given), all running PLUGIN every INTERVAL
seconds.

  gen-config.py OUT_DIR PLUGIN SERVICES INTERVAL_S MODE ADDRESS...

PLUGIN: rust or perl. MODE: cpu or storage.
"""

import sys
from pathlib import Path

SERVICES_PER_HOST = 20

COMMANDS = {
    # Rust generic SNMP plugin of centreon-plugins (rust-plugins/), the
    # HOST-RESOURCES collections of applications-protocol-snmp.
    ("rust", "cpu"): "/usr/lib/centreon/plugins/centreon-plugin-rust-snmp"
    " -H $HOSTADDRESS$ -v 2c -c public"
    " -j /usr/share/centreon/rs-collections/applications-protocol-snmp/cpu.json"
    " --warning-avg 80 --critical-avg 90",
    ("rust", "storage"): "/usr/lib/centreon/plugins/centreon-plugin-rust-snmp"
    " -H $HOSTADDRESS$ -v 2c -c public"
    " -j /usr/share/centreon/rs-collections/applications-protocol-snmp/storage.json"
    " --warning-prct 80 --critical-prct 90",
    # Packaged Perl plugin, the commands of the Linux SNMP plugin pack.
    ("perl", "cpu"): "/usr/lib/centreon/plugins/centreon_linux_snmp.pl"
    " --plugin=os::linux::snmp::plugin --mode=cpu --hostname=$HOSTADDRESS$"
    " --snmp-version=2c --snmp-community=public"
    " --warning-average=80 --critical-average=90",
    ("perl", "storage"): "/usr/lib/centreon/plugins/centreon_linux_snmp.pl"
    " --plugin=os::linux::snmp::plugin --mode=storage --hostname=$HOSTADDRESS$"
    " --snmp-version=2c --snmp-community=public"
    " --warning-usage=80 --critical-usage=90",
}


def main() -> None:
    out, plugin, services, interval, mode, *addresses = sys.argv[1:]
    services, interval = int(services), int(interval)
    out_dir = Path(out)
    out_dir.mkdir(parents=True, exist_ok=True)
    lines = [
        "define command {",
        " command_name snmp",
        f" command_line {COMMANDS[(plugin, mode)]}",
        "}",
        "define service {",
        " name tpl",
        " check_command snmp",
        " check_interval 1",
        " retry_interval 1",
        " max_check_attempts 3",
        " register 0",
        "}",
    ]
    hosts = -(-services // SERVICES_PER_HOST)
    for host in range(1, hosts + 1):
        address = addresses[(host - 1) % len(addresses)]
        lines += [
            "define host {",
            f" host_name h{host}",
            f" address {address}",
            f" _HOST_ID {host}",
            "}",
        ]
    for service in range(1, services + 1):
        host = (service - 1) // SERVICES_PER_HOST + 1
        lines += [
            "define service {",
            f" host_name h{host}",
            f" service_description s{service}",
            " use tpl",
            f" _SERVICE_ID {service}",
            "}",
        ]
    (out_dir / "objects.cfg").write_text("\n".join(lines) + "\n")
    # One interval unit of INTERVAL seconds: check_interval 1. The first
    # checks are spread over the whole interval (max_service_check_spread is
    # in minutes, as Engine's).
    (out_dir / "centengine.cfg").write_text(
        "cfg_file=objects.cfg\n"
        f"interval_length={interval}\n"
        f"max_service_check_spread={max(1, interval // 60)}\n"
        "service_check_timeout=30\n"
        "broker_module_cfg_file=module.json\n"
    )
    (out_dir / "module.json").write_text(
        '{"centreonBroker":{"poller_id":1,"poller_name":"snmp-load",'
        '"output":[{"type":"ipv4","host":"sink","port":"5669"}]}}\n'
    )
    print(f"{services} services on {hosts} hosts, {len(addresses)} agents, "
          f"{plugin} {mode} every {interval}s")


if __name__ == "__main__":
    main()
