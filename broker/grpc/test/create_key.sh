#!/bin/bash

echo "password:" $1

# Generate valid CA
openssl genrsa -passout pass:$1 -des3 -out ca_$1.key 4096
openssl req -passin pass:$1 -new -x509 -days 36500 -key ca_$1.key -out ca_$1.crt -subj  "/C=SP/ST=France/L=Toulouse/O=Test/OU=Test/CN=Root CA"

# Generate valid Server Key/Cert
openssl genrsa -passout pass:$1 -des3 -out server_$1.key 4096
openssl req -passin pass:$1 -new -key server_$1.key -out server_$1.csr -subj  "/C=SP/ST=France/L=Toulouse/O=Test/OU=Server/CN=localhost"
openssl x509 -req -passin pass:$1 -days 36500 -in server_$1.csr -CA ca_$1.crt -CAkey ca_$1.key -set_serial 01 -out server_$1.crt

# Remove passphrase from the Server Key
openssl rsa -passin pass:$1 -in server_$1.key -out server_$1.key

# Generate valid Client Key/Cert
openssl genrsa -passout pass:$1 -des3 -out client_$1.key 4096
openssl req -passin pass:$1 -new -key client_$1.key -out client_$1.csr -subj  "/C=SP/ST=France/L=Toulouse/O=Test/OU=Client/CN=localhost"
openssl x509 -passin pass:$1 -req -days 36500 -in client_$1.csr -CA ca_$1.crt -CAkey ca_$1.key -set_serial 01 -out client_$1.crt

# Remove passphrase from Client Key
openssl rsa -passin pass:$1 -in client_$1.key -out client_$1.key