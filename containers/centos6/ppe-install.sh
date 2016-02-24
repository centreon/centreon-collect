#!/bin/sh

service mysql start
service httpd start

service httpd stop
service mysql stop
