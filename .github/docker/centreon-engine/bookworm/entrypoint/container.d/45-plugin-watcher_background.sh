#!/bin/sh

PLUGINS_JSON="/etc/centreon-engine/plugins.json"
POLL_INTERVAL=30
last_hash=""

install_from_json() {
    PKGS=$(python3 - "$PLUGINS_JSON" << 'PYEOF'
import json, sys, subprocess

plugins_json = sys.argv[1]
try:
    d = json.load(open(plugins_json))
except Exception:
    sys.exit(0)

to_install = []
for pkg, ver in d.items():
    ver_str = str(ver)
    result = subprocess.run(
        ['dpkg-query', '-W', '-f=${Version}', pkg],
        capture_output=True, text=True
    )
    installed = result.stdout.strip()
    if result.returncode != 0 or not installed:
        print(f'  {pkg}: not installed -> queuing', file=sys.stderr)
        to_install.append(pkg + '-*')
    elif installed.startswith(ver_str):
        print(f'  {pkg}: up-to-date ({installed})', file=sys.stderr)
    else:
        print(f'  {pkg}: outdated ({installed} != {ver_str}) -> queuing', file=sys.stderr)
        to_install.append(pkg + '-*')

print(' '.join(to_install))
PYEOF
)
    if [ -n "$PKGS" ]; then
        echo "plugins.json changed — installing: $PKGS"
        export DEBIAN_FRONTEND=noninteractive
        sudo apt-get update -qq
        eval "sudo apt-get install -y $PKGS" > /proc/1/fd/1 2>&1 || true
    else
        echo "plugins.json changed — all plugins already up-to-date, skipping install"
    fi
}

while true; do
    sleep "$POLL_INTERVAL"
    if [ -f "$PLUGINS_JSON" ]; then
        current_hash=$(md5sum "$PLUGINS_JSON" 2>/dev/null | cut -d' ' -f1)
        if [ "$current_hash" != "$last_hash" ]; then
            last_hash="$current_hash"
            install_from_json
        fi
    fi
done
