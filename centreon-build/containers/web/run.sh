#!/bin/sh

#set -e
set -x

service mysql start
service httpd start
service cbd start
service centengine start
service centcore start
bash
service centcore stop
service centengine stop
service cbd stop
service httpd stop
service mysql stop
