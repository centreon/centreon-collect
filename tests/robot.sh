#!/bin/bash

getent group centreon-engine || groupadd centreon-engine

#sed -i -r 's/(\$\{DBUserRoot\}\s*)root_centreon/\1root/g' resources/db_variables.resource

# Core dumps. Raising the limit is what really matters, and it needs no
# privilege. Setting the pattern only moves the cores out of the source tree:
# with the default 'core' pattern and kernel.core_uses_pid=1 they land in
# ./core.<pid>, which 'Ctn Coredump Info' knows how to find anyway.
ulimit -c unlimited || echo "WARNING: cannot raise the core file size limit, no core dump will be generated."
expected_pattern='/tmp/core.%p'
current_pattern=$(cat /proc/sys/kernel/core_pattern)
if [ "$current_pattern" != "$expected_pattern" ] ; then
  if ! sysctl -w "kernel.core_pattern=$expected_pattern" > /dev/null 2>&1 ; then
    echo "NOTE: kernel.core_pattern is '$current_pattern' and could not be changed"
    echo "      (in a rootless podman container /proc/sys is read-only). Core dumps will be"
    echo "      written next to the sources instead of /tmp, and they are big. To move them,"
    echo "      run this on the host as root (the setting is global to the machine):"
    echo "          sysctl -w kernel.core_pattern=$expected_pattern"
  fi
fi

robot -L TRACE $*
rep=$(date +%s)
mkdir $rep
mv report.html log.html output.xml $rep
if [ -f processing.log ] ; then
  mv processing.log $rep
fi
