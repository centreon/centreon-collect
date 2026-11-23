#!/bin/sh

# engine-context.json holds the keys that decrypt the encrypt:: values of the
# delivered configuration (APP_SECRET and SALT of the central web). Written
# from env when the file is absent or still the empty image placeholder. A
# file with a non-empty app_secret (delivered or mounted) is never touched.

CTX="/etc/centreon-engine/engine-context.json"

if [ -z "${APP_SECRET:-}" ] || [ -z "${SALT:-}" ]; then
    return 0 2>/dev/null || exit 0
fi
if [ -f "$CTX" ] && ! grep -q '"app_secret": *""' "$CTX"; then
    return 0 2>/dev/null || exit 0
fi

was_tracing=0
case "$-" in *x*) was_tracing=1 ;; esac
set +x
# Sourced by container.sh: keep the umask change in a subshell.
(
    umask 027
    printf '{"app_secret":"%s","salt":"%s"}\n' "$APP_SECRET" "$SALT" > "$CTX.tmp"
    chmod 0640 "$CTX.tmp"
    mv "$CTX.tmp" "$CTX"
)
if [ "$was_tracing" = "1" ]; then
    set -x
fi
echo "engine-context: written to $CTX from APP_SECRET/SALT"
