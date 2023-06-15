#!/bin/sh

printf "\033[32m Stop the service unit\033[0m\n"
systemctl stop cbd.service ||:

