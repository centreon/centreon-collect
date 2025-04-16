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

dnf install -y /tmp/packages-centreon/centreon-*.rpm centreon-gorgone

# temporary fix and inspired by from centreon-poller-postinstall.sh
usermod centreon-engine -a -G centreon,centreon-broker,centreon-gorgone
usermod centreon-broker -a -G centreon
usermod centreon -a -G centreon-engine,centreon-broker
usermod centreon-gorgone -a -G centreon-engine,centreon-broker,centreon

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
