#!/bin/sh
# Runs the scheduler against a real cbd: the broker image of this branch
# with a TCP input and a Lua output that dumps every decoded event, then the
# ignored test `real_cbd`, which reads the dump. Removes the container after.
#
#   crates/centreon-engine/tests/real-cbd/run.sh   # from gorgone-rs/
#
# BROKER_IMAGE overrides the image (default centreon-broker-trixie:gorgone-poc).
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
grep -E "peer is using protocol|corrupted|unknown event|received stop|acknowledgement for" \
  "$work/cbd.log" | tail -n 20 || true
exit "$status"
