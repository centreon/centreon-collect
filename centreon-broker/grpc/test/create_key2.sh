#!/bin/bash

#inspired from https://stackoom.com/en/question/2kS6T
echo "password:" $1
mypass=$1

echo Generate server key:
openssl genrsa -passout pass:$mypass -des3 -out server_2_$mypass.key 4096

echo Generate server signing request:
openssl req -passin pass:$mypass -new -key server_2_$mypass.key -out server_2_$mypass.csr -subj "/C=US/ST=FR/L=Toulouse/O=Google/OU=youtube/CN=localhost"

echo Self-sign server certificate:
openssl x509 -req -passin pass:$mypass -days 36500 -in server_2_$mypass.csr -signkey server_2_$mypass.key -set_serial 01 -out server_2_$mypass.crt

echo Remove passphrase from server key:
openssl rsa -passin pass:$mypass -in server_2_$mypass.key -out server_2_$mypass.key
