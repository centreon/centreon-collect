#!/bin/sh

# rpm %posttrans only (el8, el9, el10): deb has no equivalent and restarts
# centagent from its postinst.
#
# The restart is done here rather than in %post because %posttrans is the
# only scriptlet that runs after the whole transaction:
#  - the %preun of the replaced package has already run. Up to 24.10.x,
#    25.10.9 and 26.10.0 it stops centagent unconditionally, even on upgrade,
#    which left the agent stopped after a package update.
#  - centreon-monitoring-agent-selinux, when part of the same transaction, has
#    already loaded its policy module.
#
# %posttrans is not run on package removal.
systemctl daemon-reload ||:
systemctl restart centagent.service ||:
