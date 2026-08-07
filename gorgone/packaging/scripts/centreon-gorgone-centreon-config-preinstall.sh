#!/bin/bash

# centreon-gorgone-centreon-config ships config files owned by centreon-gorgone, but no
# longer depends on the centreon-gorgone package (dependency direction was inverted:
# centreon-gorgone now depends on centreon-gorgone-centreon-config, not the other way
# around). Without this, the centreon-gorgone user/group may not exist yet when these
# files are laid down, leaving them owned by root and unreadable by the gorgone daemon
# and by apache (via the centreon-gorgone group).
if ! getent group centreon-gorgone > /dev/null 2>&1; then
  groupadd -r centreon-gorgone
fi

if ! getent passwd centreon-gorgone > /dev/null 2>&1; then
  useradd -g centreon-gorgone -m -d /var/lib/centreon-gorgone -r centreon-gorgone 2> /dev/null
fi
