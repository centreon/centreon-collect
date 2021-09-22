#!/bin/sh

#
# Update repo with new RPMs.
#

PID_FILE="/tmp/updaterepo.pid"
REPO="$1"

if [ -n "$REPO" ] ; then
  success=0
  while [ "$success" = 0 ] ; do
    if [ -f "$PID_FILE" ] ; then
      concurrent=`cat $PID_FILE | head -n 1`
      while [ -d "/proc/$concurrent" ] ; do
        sleep 1
      done
    else
      echo $$ >> "$PID_FILE"
      concurrent=`cat $PID_FILE | head -n 1`
      if [ "$concurrent" = $$ ] ; then
        createrepo "/srv/yum/$REPO"
        rm -f "$PID_FILE"
        success=1
      fi
    fi
  done
fi

#
# Clean after ourselves.
#
rm -f "$0"
