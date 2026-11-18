#!/bin/sh
# Runs the scheduler against a real cbd: the broker image of this branch
# with a TCP input and a Lua output that dumps every decoded event, then the
# ignored test `real_cbd`, which reads the dump. Removes the container after.
#
#   crates/centreon-engine/tests/real-cbd/run.sh   # from gorgone-rs/
#
# BROKER_IMAGE overrides the image (default centreon-broker-trixie:gorgone-poc).
# TLS=1 makes the cbd input require TLS ("tls": "yes", EC certificates of a
# CA made here with openssl): the engine must negotiate the extension and
# connect as the TLS client with its own certificate (files, or inline in a
# version 5 document with an encrypt:: key), and a poller certificate of
# another CA is refused.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
image=${BROKER_IMAGE:-centreon-broker-trixie:gorgone-poc}
name=centreon-engine-cbd-$$
work=$(mktemp -d)
cleanup() {
  docker rm -f "$name" >/dev/null 2>&1 || true
  rm -rf "$work"
}
trap cleanup EXIT INT TERM

cp "$here/cbd.json" "$here/dump.lua" "$work/"
if [ "${TLS:-0}" = "1" ]; then
  # Certificates as centreon-web's managed PKI issues them (EC P-256, CA
  # constraints, serverAuth for cbd, clientAuth for the pollers), plus a
  # poller of another CA that cbd must refuse.
  (
    cd "$work"
    ec="-newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes"
    ca_ext="basicConstraints=critical,CA:TRUE
keyUsage=critical,keyCertSign,cRLSign"
    for ca in ca rogue-ca; do
      # shellcheck disable=SC2086
      openssl req -x509 $ec -days 1 -subj "/CN=$ca" -addext "$(echo "$ca_ext" | head -n 1)" \
        -addext "$(echo "$ca_ext" | tail -n 1)" -keyout "$ca.key" -out "$ca.pem" 2>/dev/null
    done
    issue() { # name ca extended-key-usage san
      # shellcheck disable=SC2086
      openssl req $ec -subj "/CN=$1" -keyout "$1.key" -out "$1.csr" 2>/dev/null
      printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=%s\n%s\n' \
        "$3" "$4" >"$1.ext"
      openssl x509 -req -in "$1.csr" -CA "$2.pem" -CAkey "$2.key" -CAcreateserial \
        -days 1 -extfile "$1.ext" -out "$1.pem" 2>/dev/null
    }
    issue central ca serverAuth "subjectAltName=DNS:central"
    issue poller ca clientAuth ""
    issue inline ca clientAuth ""
    issue rogue rogue-ca clientAuth ""
    chmod 644 ./*.pem ./*.key
  )
  sed -i 's#"tls": "auto",#"tls": "yes", "private_key": "/work/central.key", "public_cert": "/work/central.pem", "ca_certificate": "/work/ca.pem",#' \
    "$work/cbd.json"
  sed -i 's#"tcp": "debug",#"tcp": "debug", "tls": "debug",#' "$work/cbd.json"
  export GORGONE_TEST_CBD_TLS="$work"
fi
# cbd runs as its own user in the image.
chmod 777 "$work"
chmod 644 "$work/cbd.json" "$work/dump.lua"

docker run -d --name "$name" -p 127.0.0.1::5669 -v "$work:/work" \
  --entrypoint /usr/sbin/cbd "$image" /work/cbd.json >/dev/null
port=$(docker port "$name" 5669/tcp | head -n 1 | sed 's/.*://')
for _ in $(seq 1 60); do
  if grep -q "central-broker-master-input" "$work/cbd.log" 2>/dev/null; then
    break
  fi
  sleep 1
done
sleep 1

status=0
GORGONE_TEST_CBD_ADDRESS="127.0.0.1:$port" GORGONE_TEST_CBD_EVENTS="$work/events.json" \
  cargo test -p centreon-engine --test real_cbd -- --ignored --nocapture || status=$?
if [ "$status" -ne 0 ]; then
  echo "--- cbd log (end) ---"
  tail -n 80 "$work/cbd.log" || true
fi
echo "--- cbd BBDO lines about the poller ---"
grep -E "peer is using protocol|corrupted|unknown event|received stop|acknowledgement for|extensions|handshake|credentials|certificate" \
  "$work/cbd.log" | tail -n 20 || true
exit "$status"
