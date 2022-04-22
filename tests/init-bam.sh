#!/bin/bash

mysql -u root -pcentreon -e "drop database centreon"
mysql -u root -pcentreon < resources/centreon.sql
