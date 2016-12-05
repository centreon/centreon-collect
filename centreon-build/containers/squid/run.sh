#!/bin/sh

set -x

httpd -k start
squid -f /etc/squid/squid.conf
tailf /var/log/httpd/error_log
