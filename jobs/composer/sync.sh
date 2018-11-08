#!/bin/sh

# Script has to be setuid.
cd /srv/satis/satis/ && ./bin/satis build satis.json www/
