#!/bin/sh

# rpm %posttrans only (el8, el9, el10): deb has no equivalent and restarts
# centengine from its postinst.
#
# The restart is done here rather than in %post because %posttrans is the
# only scriptlet that runs after the whole transaction:
#  - every package of the upgrade is laid down, so cbmod.so (shipped by
#    centreon-broker-cbmod) and the broker modules (centreon-broker-core) or
#    the opentelemetry module have the same version. Otherwise centengine exits
#    with "modules: version mismatch".
#  - the %preun of the replaced package has already run. Up to 24.10.x,
#    25.10.14 and 26.10.0 it stops centengine unconditionally, even on upgrade.
#
# %posttrans is not run on package removal.
systemctl daemon-reload ||:
systemctl restart centengine.service ||:
