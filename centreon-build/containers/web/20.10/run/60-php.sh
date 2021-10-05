#!/bin/sh

if [ -x /opt/rh/rh-php72/root/usr/sbin/php-fpm ] ; then
  /opt/rh/rh-php72/root/usr/sbin/php-fpm
else
  /usr/sbin/php-fpm
fi
