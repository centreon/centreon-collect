#!/bin/sh

# Arguments.
tpl=
hosts=1
instance='Central'
groups=
user='admin'
password='centreon'
dico='dico.txt'
confirm=0

#
# Process arguments.
#
while [ $# -gt 1 ] ; do
  if [ "$1" = '-t' ] ; then
    shift
    tpl="$1"
    shift
  elif [ "$1" = '-h' ] ; then
    shift
    hosts="$1"
    shift
  elif [ "$1" = '-i' ] ; then
    shift
    instance="$1"
    shift
  elif [ "$1" = '-u' ] ; then
    shift
    user="$1"
    shift
  elif [ "$1" = '-p' ] ; then
    shift
    password="$1"
    shift
  elif [ "$1" = '-g' ] ; then
    shift
    groups="$1"
    shift
  elif [ "$1" = '-c' ] ; then
    confirm=1
    shift
  else
    echo "USAGE: genhosts.sh -t <HOSTTEMPLATE> [-c] [-h <HOSTCOUNT:1>] [-i <INSTANCE:Central>] [-u <USER:admin>] [-p <PASSWORD:centreon>] [-g <GROUPS>]"
    exit 1
  fi
done

#
# Insert hosts.
#
if [ -n "$tpl" -a \( $hosts -ge 1 \) ] ; then
  while [ $hosts -ge 1 ] ; do
    lines=`wc -l < $dico`
    pos=`shuf -i 1-$lines -n 1`
    name=`head -n $pos < $dico | tail -n 1`
    if [ $confirm = 1 ] ; then
      centreon -u "$user" -p "$password" -o HOST -a ADD -v "$name;$name;localhost;$tpl;$instance;$groups"
    else
      echo centreon -u "$user" -p "$password" -o HOST -a ADD -v "$name;$name;localhost;$tpl;$instance;$groups"
    fi
    hosts=$(($hosts-1))
    echo "$hosts hosts to create"
  done
else
  echo "USAGE: genhosts.sh -t <HOSTTEMPLATE> [-c] [-h <HOSTCOUNT:1>] [-i <INSTANCE:Central>] [-u <USER:admin>] [-p <PASSWORD:centreon>] [-g <GROUPS>]"
  exit 1
fi
