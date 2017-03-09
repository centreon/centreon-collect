#!/bin/sh

set -e
set -x

Xvfb :99 &
export DISPLAY=:99
java -jar selenium-server-standalone.jar
