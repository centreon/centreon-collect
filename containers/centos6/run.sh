#!/bin/sh

service mysql start
service httpd start
bash
service httpd stop
service mysql stop
