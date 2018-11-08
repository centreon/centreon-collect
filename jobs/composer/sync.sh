#!/bin/sh

set -e
set -x

cd /srv/satis/satis/
./bin/satis build satis.json www/
