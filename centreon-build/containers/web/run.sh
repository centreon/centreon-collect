#!/bin/sh

#set -e
set -x

service mysql start
service cbd start
service centengine start
service centcore start
service httpd start
bash
service httpd stop
service centcore stop
service centengine stop
service cbd stop
service mysql stop
