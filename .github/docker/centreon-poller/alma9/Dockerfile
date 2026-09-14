FROM almalinux:9

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

COPY --chmod=755 ./.github/docker/centreon-poller/scripts/systemctl /bin/systemctl

RUN --mount=type=bind,src=packages-centreon,dst=/tmp/packages-centreon \
    --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_USERNAME \
    --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_PASSWORD \
    bash -e <<EOF

echo 'install_weak_deps=False' >> /etc/dnf/dnf.conf

dnf install -y dnf-plugins-core

dnf config-manager --set-enabled crb

dnf install -y epel-release initscripts

if [[ "${IS_CLOUD}" == "true" ]]; then
  dnf config-manager --add-repo https://$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_USERNAME):$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_PASSWORD)@packages.centreon.com/rpm-standard-internal/${VERSION}/el9/centreon-${VERSION}-internal.repo
  sed -i "s#packages.centreon.com/rpm-standard-internal#$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_USERNAME):$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_PASSWORD)@packages.centreon.com/rpm-standard-internal#" /etc/yum.repos.d/centreon-${VERSION}-internal.repo
else
  dnf config-manager --add-repo https://packages.centreon.com/rpm-standard/${VERSION}/el9/centreon-${VERSION}.repo
fi
dnf config-manager --set-enabled 'centreon*'

if [[ "$STABILITY" == "testing" ]]; then
  dnf config-manager --set-disabled 'centreon*unstable*'
elif [[ "$STABILITY" == "stable" ]]; then
  dnf config-manager --set-disabled 'centreon*unstable*' --set-disabled 'centreon*testing*'
fi

dnf install -y /tmp/packages-centreon/centreon-*.rpm centreon-gorgone centreon-common

usermod -aG centreon-engine centreon-gorgone
usermod -aG centreon-broker centreon-gorgone

# The reverse membership is also needed: gorgone (re)generates poller
# config as centreon-gorgone:centreon-gorgone, mode 0660. Without
# centreon-engine/centreon-broker in the centreon-gorgone group,
# centengine gets "Permission denied" reading centengine.cfg after the
# first CFGMOVE and fails to restart with the pushed-down config (it
# keeps running the stale one baked at image build time until killed).
usermod -aG centreon-gorgone centreon-engine
usermod -aG centreon-gorgone centreon-broker

# sudoersCentreon's NOPASSWD rules target the "centreon" group
# (User_Alias CENTREON=%centreon), not centreon-gorgone directly, so
# without this centreon-gorgone hits a real password prompt when
# gorgone shells out to `sudo systemctl restart centengine` and the
# restart signal never gets through. centreon-gorgone's own postinstall
# (gorgone/packaging/scripts/centreon-gorgone-postinstall.sh) does add
# this, but relies on the centreon user/group already existing, which
# isn't guaranteed by RPM transaction ordering in a single dnf install.
usermod -aG centreon centreon-gorgone
usermod -aG centreon-gorgone centreon

# The engine package's default centengine.cfg ships without a
# broker_module_cfg_file directive (unlike its bare-metal RPM default),
# so centengine exits at startup ("No module configuration file provided")
# before the poller ever gets a chance to register with the central.
if ! grep -q '^broker_module_cfg_file=' /etc/centreon-engine/centengine.cfg; then
  echo 'broker_module_cfg_file=/etc/centreon-broker/central-module.json' >> /etc/centreon-engine/centengine.cfg
fi

# gorgone's legacycmd module defaults cache_dir to /var/cache/centreon
# (used to receive the config files pushed by CFGMOVE from the central).
# On a full centreon-web install, the centreon package's postinstall
# creates this directory; the poller only installs centreon-gorgone, so
# it's never created here and every config push fails with "Copy
# processing - MD5 does not match" (open() on the destination fails).
install -d -m 0770 -o centreon-gorgone -g centreon-gorgone /var/cache/centreon

# Same problem for /etc/centreon: normally created by the centreon-web
# package (e.g. for centreon_vmware.json pushed by gorgone's legacycmd
# module), never installed on the poller, so it's missing here too and
# any such copy fails with "No such file or directory".
install -d -m 0770 -o centreon-gorgone -g centreon-gorgone /etc/centreon

systemctl stop gorgoned
systemctl stop centengine

dnf clean all

EOF

COPY --chmod=755 ./.github/docker/centreon-poller/alma9/entrypoint /usr/share/centreon
COPY --chmod=755 ./.github/docker/centreon-poller/init/* /etc/init.d/
COPY --chmod=600 ./.github/docker/centreon-poller/sudoersCentreon /etc/sudoers.d/centreon

# grpc central-module
EXPOSE 51003

# bbdo central-broker
EXPOSE 5669
# grpc central-broker
EXPOSE 51001

# bbdo central-rrd
EXPOSE 5670
# grpc central-rrd
EXPOSE 51002

ENTRYPOINT ["/usr/share/centreon/container.sh"]
