#!/bin/sh

# Self-generated PKI of the central Broker (gorgone-less PoC).
#
# The pollers get ca.crt through the poller creation API (cbd GET /v1/ca),
# never through the BBDO stream. Rules:
# - central.crt and central.key present: nothing is generated (corporate PKI
#   or already generated; a read-only mount is fine), except with ca.key
#   present (self-generated PKI) and a SAN list that differs from
#   BROKER_TLS_SAN: the server pair is renewed with the same CA.
# - ca.crt and ca.key present: a new server pair is signed by that CA
#   (renewal: delete central.* and restart, the CA never changes).
# - nothing present: generate the CA, then the server pair.
# - ca.crt without ca.key and no server pair: error, nothing can be signed.
#
# Env: BROKER_TLS_SELF_CA (1, 0 disables), BROKER_TLS_SAN (comma-separated
# names or IPs, default centreon-broker, the first one is also the CN),
# BROKER_CERTS_DIR (/etc/centreon-broker/certs).

[ "${BROKER_TLS_SELF_CA:-1}" = "1" ] || exit 0

DIR="${BROKER_CERTS_DIR:-/etc/centreon-broker/certs}"
SAN_LIST="${BROKER_TLS_SAN:-centreon-broker}"

# Wanted SAN list, as openssl writes it: DNS:<name> / IP:<address>.
SAN=""
CN=""
OLD_IFS="$IFS"
IFS=","
for name in $SAN_LIST; do
    name=$(echo "$name" | tr -d ' ')
    [ -n "$name" ] || continue
    [ -n "$CN" ] || CN="$name"
    case "$name" in
        *[!0-9.]*) entry="DNS:$name" ;;
        *) entry="IP:$name" ;;
    esac
    SAN="${SAN:+$SAN,}$entry"
done
IFS="$OLD_IFS"

# SAN list of a certificate, normalised like $SAN.
cert_san() {
    openssl x509 -in "$1" -noout -ext subjectAltName 2>/dev/null \
        | sed -n '2p' | sed 's/IP Address:/IP:/g' | tr -d ' '
}

if [ -f "$DIR/central.crt" ] && [ -f "$DIR/central.key" ]; then
    # Self-generated PKI (ca.key here): renew the server pair when
    # BROKER_TLS_SAN changed, with the same CA (fingerprint unchanged). A
    # mounted pair without ca.key (corporate PKI) is never touched.
    OLD_SAN=$(cert_san "$DIR/central.crt")
    if [ ! -f "$DIR/ca.key" ] || [ ! -f "$DIR/ca.crt" ] || [ "$OLD_SAN" = "$SAN" ]; then
        echo "tls: server certificate present in $DIR, nothing generated"
        exit 0
    fi
    if [ ! -w "$DIR" ]; then
        echo "tls: WARNING: SAN changed ($OLD_SAN -> $SAN) but $DIR is not" \
             "writable, keeping the current server certificate" >&2
        exit 0
    fi
    echo "tls: SAN changed ($OLD_SAN -> $SAN), server certificate renewed"
fi

if [ ! -d "$DIR" ] || [ ! -w "$DIR" ]; then
    echo "tls: ERROR: $DIR is not writable by $(id -un), cannot generate the PKI" >&2
    exit 1
fi

umask 077
set -e

if [ ! -f "$DIR/ca.crt" ] && [ ! -f "$DIR/ca.key" ]; then
    echo "tls: generating the local CA in $DIR"
    openssl req -x509 -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes \
        -keyout "$DIR/ca.key.tmp" -out "$DIR/ca.crt.tmp" -days 3650 -sha256 \
        -subj "/CN=Centreon Broker local CA" \
        -addext "basicConstraints=critical,CA:TRUE" \
        -addext "keyUsage=critical,keyCertSign,cRLSign" 2>&1
    chmod 0600 "$DIR/ca.key.tmp"
    chmod 0644 "$DIR/ca.crt.tmp"
    mv "$DIR/ca.key.tmp" "$DIR/ca.key"
    mv "$DIR/ca.crt.tmp" "$DIR/ca.crt"
elif [ ! -f "$DIR/ca.crt" ] || [ ! -f "$DIR/ca.key" ]; then
    echo "tls: ERROR: $DIR holds only one of ca.crt/ca.key and no" \
         "central.crt/central.key: mount the full PKI or remove it" >&2
    exit 1
fi

echo "tls: generating the server certificate ($SAN) signed by $DIR/ca.crt"
EXT="$DIR/central.ext"
cat > "$EXT" <<EOF
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=$SAN
EOF
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes \
    -keyout "$DIR/central.key.tmp" -out "$DIR/central.csr" -subj "/CN=$CN" 2>&1
openssl x509 -req -in "$DIR/central.csr" -CA "$DIR/ca.crt" -CAkey "$DIR/ca.key" \
    -CAcreateserial -out "$DIR/central.crt.tmp" -days 825 -sha256 \
    -extfile "$EXT" 2>&1
rm -f "$EXT" "$DIR/central.csr"
chmod 0644 "$DIR/central.crt.tmp"
chmod 0600 "$DIR/central.key.tmp"
mv "$DIR/central.key.tmp" "$DIR/central.key"
mv "$DIR/central.crt.tmp" "$DIR/central.crt"
echo "tls: CA fingerprint sha256 $(openssl x509 -in "$DIR/ca.crt" -noout -fingerprint -sha256 | cut -d= -f2 | tr -d : | tr 'A-F' 'a-f')"
