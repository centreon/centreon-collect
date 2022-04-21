#!/bin/bash

mysql -u root -pcentreon -e "drop database centreon"
mysql -u root -pcentreon < resources/centreon.sql
#mysql -u root -pcentreon -e "SET GLOBAL FOREIGN_KEY_CHECKS=0"
#mysql centreon -u root -pcentreon < ../broker/simu/docker/bam.sql
#mysql -u root -pcentreon -e "SET GLOBAL FOREIGN_KEY_CHECKS=1"
