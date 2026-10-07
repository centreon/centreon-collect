#!/bin/sh

# CA for the CMA/OpenTelemetry module. Real .deb installs generate it via
# postinstall (centengine -k, packaging/centreon-collect/scripts/
# centreon-engine-daemon-postinstall.sh) - dpkg-deb -x in this image's build
# skips postinstall entirely, so it's never created. Generated once here at
# runtime instead, into the baked (now-writable) /etc/pki/centreon-engine.
CA_DIR="/etc/pki/centreon-engine"
if [ ! -f "$CA_DIR/default_cma_ca.crt" ] || [ ! -f "$CA_DIR/default_cma_ca.key" ]; then
    # centengine -k always exits 1 even on success (main.cc never sets
    # retval after gen_cma_key()) - check for the files it creates, not $?.
    /usr/sbin/centengine -k || true
fi
