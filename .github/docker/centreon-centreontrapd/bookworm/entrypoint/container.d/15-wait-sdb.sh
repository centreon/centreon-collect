#!/bin/sh
SDB="${TRAP_SDB_PATH:-/etc/snmp/centreon_traps/centreontrapd.sdb}"
TIMEOUT="${SDB_WAIT_TIMEOUT:-300}"
ELAPSED=0

echo "=== Waiting for centreontrapd.sdb ==="
echo "Path   : $SDB"
echo "Timeout: ${TIMEOUT}s"

while [ ! -f "$SDB" ] && [ "$ELAPSED" -lt "$TIMEOUT" ]; do
    echo "Waiting for $SDB... (${ELAPSED}s/${TIMEOUT}s)"
    sleep 5
    ELAPSED=$((ELAPSED + 5))
done

if [ -f "$SDB" ]; then
    # Gorgone writes the .sdb as centreon:centreon with mode 0664 (per legacycmd.pm)
    # centreon-trapd is in the centreon group so it can read it
    echo "✓ centreontrapd.sdb found at $SDB"
    ls -lah "$SDB" || true
else
    echo "WARNING: centreontrapd.sdb not yet available after ${TIMEOUT}s"
    echo "centreontrapd will start but OID lookups will fail until gorgone delivers the .sdb"
fi
echo ""
