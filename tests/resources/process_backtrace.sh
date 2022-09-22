#!/bin/bash

usage()
{
        cat <<!
        process_backtrace.sh <pid> : extract process stack
        process_backtrace <bin> <core> : extract process stack from core
!
}

if [ $# -ne 1 ] &&  [ $# -ne 2 ]
then
        usage
        exit 1
fi

# -- Recuperation des arguments
while getopts \? c
do
        case $c in
        *) usage ; exit 1 ;;
        esac
done
shift `expr $OPTIND - 1`

if [ $# -eq 1 ]
then
    pid=$1
    out=/tmp/bt.$pid
    gdbopt="-p $1"
fi

if [ $# -eq 2 ]
then
    pid=$1
    gdbopt="$1 $2"
    out=/tmp/bt.$(basename $2)
fi

(echo thread apply all bt full 20; echo q) > ${out}.txt

gdb -batch -x ${out}.txt $gdbopt > $out  2>&1
cat $out
[ ! -z "$pid" ] && kill -CONT $pid
#rm ${out}.txt