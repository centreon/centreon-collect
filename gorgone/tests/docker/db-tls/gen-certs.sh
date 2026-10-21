#!/bin/bash
# Generate a test CA, a server certificate for the "db" and "mariadb" hosts and a second unrelated
# CA (wrong-ca.pem) used to check that the server certificate is really verified.
# usage: gen-certs.sh [output directory]   (default: certs/ next to this script)
set -e
out="${1:-$(dirname "$0")/certs}"
mkdir -p "$out"
cd "$out"

openssl req -x509 -newkey rsa:2048 -nodes -days 365 -subj "/CN=Test CA" -keyout ca-key.pem -out ca.pem 2>/dev/null
openssl req -newkey rsa:2048 -nodes -subj "/CN=db" -keyout server-key.pem -out server.csr 2>/dev/null
printf "subjectAltName=DNS:db,DNS:mariadb,DNS:localhost,IP:127.0.0.1\n" > server.ext
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca-key.pem -CAcreateserial -days 365 -extfile server.ext -out server.pem 2>/dev/null
openssl req -x509 -newkey rsa:2048 -nodes -days 365 -subj "/CN=Other CA" -keyout wrong-ca-key.pem -out wrong-ca.pem 2>/dev/null
rm -f server.csr server.ext ca.srl
# read by the database server (non root user) and gorgone containers
chmod 644 ./*.pem
ls -1 "$PWD"
