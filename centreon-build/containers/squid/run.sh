#!/bin/sh

set -x

httpd -k start
service squid start
tailf /var/log/squid/access.log
