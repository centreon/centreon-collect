#!/bin/sh

set -e
set -x

# Install required build dependencies for all Centreon projects.
xargs zypper --non-interactive install --download-only < /tmp/build-dependencies.txt

# Install fake yum-builddep binary to install build dependencies.
cat > /usr/local/bin/yum-builddep <<EOF
#!/bin/sh
zypper --non-interactive install `rpmspec -q --buildrequires $3`
EOF
chmod +x /usr/local/bin/yum-builddep
