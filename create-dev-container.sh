#!/usr/bin/env bash
#
# create-dev-container.sh -- create a podman dev container able to run the
# centreon-collect robot test suite. Run with --help for arguments, defaults
# and examples.
#
# Recipe derived from:
#   .github/docker/Dockerfile.centreon-collect-mariadb-trixie-test
#   .github/scripts/collect-setup-database.sh
#   tests/README.md  (the official uv procedure)
#   + direct inspection of the reference dev container
#
set -euo pipefail

# -- Help ---------------------------------------------------------------------
usage() {
  cat <<'USAGE'
Usage: create-dev-container.sh [-h|--help] [container-name] [worktree]

Create a Debian 13 (trixie) podman container ready to run the centreon-collect
robot test suite: build toolchain, MariaDB with the centreon and
centreon_storage databases loaded, robot venv (uv, the container's python3) and
gRPC stubs.

Arguments (positional, all optional)
  container-name  Name of the container to create. Default: centreon-dev
                  WARNING: an existing container with that name is destroyed
                  and recreated.
  worktree        Subdirectory of the git worktree to test, or '.' for the main
                  repository. Selects where the robot venv
                  (<worktree>/tests/robotframework) and the gRPC stubs are
                  created, and which SQL dumps are loaded.
                  Default: the worktree the script is run from.

Environment variables
  CACHE           Host directory mounted on /root/.cache in the container.
                  Default: <repository-parent>/cache

The main repository is derived from git (`rev-parse --git-common-dir`, so it
works from any worktree) and mounted on /work. No path is hardcoded.

Examples
  # from the worktree to test, with an explicit container name
  cd ~/Projets/centreon-collect/dt-broker-develop && ../create-dev-container.sh dt-broker-dev

  # worktree given explicitly, cache elsewhere
  CACHE=/srv/cache ./create-dev-container.sh dev-develop develop

  # test the main repository
  ./create-dev-container.sh main-dev .

The container holds NO centreon binary: build on the host
(ninja -C <worktree>/build), then install inside the container
(cmake --install <worktree>/build). The script prints a reminder when it ends.
USAGE
}

for arg in "$@"; do
  case "${arg}" in
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      echo "Error: unknown option '${arg}'." >&2
      echo >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [ "$#" -gt 2 ]; then
  echo "Error: too many arguments ($#), 2 at most." >&2
  echo >&2
  usage >&2
  exit 2
fi

# -- Parameters ---------------------------------------------------------------
CONTAINER="${1:-centreon-dev}"                    # name of the new container
IMAGE="debian:trixie"                             # same base as the reference container
WORK="/work"                                      # where the sources are mounted

# Root of the main repository: that is what gets mounted on /work, and what
# makes the linked worktrees visible. --git-common-dir returns it from any
# worktree; start from the current directory when it is inside the repository,
# otherwise from the script's own directory.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GIT_FROM="$(git -C "${PWD}" rev-parse --show-toplevel 2>/dev/null || echo "${SCRIPT_DIR}")"
SRC="$(git -C "${GIT_FROM}" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
SRC="${SRC%/}"; SRC="${SRC%/.git}"
if [ -z "${SRC}" ] || [ ! -d "${SRC}" ]; then
  echo "Error: no git repository found from ${PWD} nor from ${SCRIPT_DIR}." >&2
  exit 1
fi

# Worktree to test: 2nd argument, otherwise the one the script is run from.
TOP="$(git -C "${GIT_FROM}" rev-parse --show-toplevel)"
if [ "${TOP}" = "${SRC}" ]; then
  DEFAULT_WORKTREE="."
else
  DEFAULT_WORKTREE="${TOP#"${SRC}"/}"
fi
WORKTREE="${2:-${DEFAULT_WORKTREE}}"              # '.' = main repository

# Shared cache, mounted on /root/.cache as in the reference container.
CACHE="${CACHE:-$(dirname "${SRC}")/cache}"

# Root of the tested worktree, as seen from the container.
if [ "${WORKTREE}" = "." ]; then
  REPO_DIR="${WORK}"
else
  REPO_DIR="${WORK}/${WORKTREE}"
fi
TESTS_DIR="${REPO_DIR}/tests"

if [ ! -d "${SRC}/${WORKTREE}/tests" ]; then
  echo "Error: ${SRC}/${WORKTREE}/tests not found (worktree '${WORKTREE}')." >&2
  echo "Available worktrees:" >&2
  git -C "${SRC}" worktree list >&2
  exit 1
fi

echo ">>> Repository: ${SRC}"
echo ">>> Worktree  : ${WORKTREE}"
echo ">>> Cache     : ${CACHE}"

echo ">>> (1/6) Creating container ${CONTAINER} (${IMAGE}) -- worktree ${WORKTREE}"
mkdir -p "${CACHE}"
podman rm -f "${CONTAINER}" 2>/dev/null || true
# --privileged and the same mounts as the reference container (pasta network by
# default).
podman run -d --name "${CONTAINER}" \
  --hostname "${CONTAINER}" \
  --privileged \
  -v "${SRC}:${WORK}" \
  -v "${CACHE}:/root/.cache" \
  "${IMAGE}" sleep infinity

# Helper: run a bash snippet inside the container (fail-fast).
run() { podman exec -i "${CONTAINER}" bash -euo pipefail -c "$*"; }

echo ">>> (2/6) System packages and build toolchain"
# Union of the trixie CI Dockerfile and the packages installed by hand in the
# reference container (dev: gdb/lldb/valgrind/mold/ninja, -dev libraries, ...).
# heaptrack and sqlite3 are required by the allocation measurement campaigns
# (tests/benchmarks, EALLOC4 profile on cbd).
# NB: the CI Dockerfile removes gcc/g++/python3-dev at the end of the build,
#     since it produces a test image; they are KEPT here, this is a dev
#     container.
run '
  export DEBIAN_FRONTEND=noninteractive
  apt-get update
  apt-get install -y \
    curl wget git zip unzip zstd tar ca-certificates gnupg sudo locales \
    g++ gcc gdb lldb valgrind \
    cmake ninja-build mold sccache pkg-config \
    make autoconf automake libtool m4 \
    python3 python3-dev python3-pip python3-venv \
    perl perl-base libhttp-daemon-ssl-perl libjson-perl \
    liblua5.3 liblua5.3-dev libmariadb3 libmariadb-dev libperl-dev \
    libcurl4-openssl-dev libgcrypt20-dev libgnutls28-dev libssh2-1-dev librrd-dev \
    rrdcached rrdtool \
    openssh-server dsniff psmisc strace tcpdump inotify-tools iptables \
    ripgrep htop tig nmon \
    heaptrack sqlite3 \
    mariadb-server mariadb-client
  apt-get clean
  localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8 || true
'

echo ">>> (3/6) Centreon service users"
run '
  id centreon-engine >/dev/null 2>&1 || useradd -d /var/lib/centreon-engine -r centreon-engine
  id centreon-broker >/dev/null 2>&1 || useradd -d /var/lib/centreon-broker -r centreon-broker
'

echo ">>> (4/6) MariaDB and the centreon / centreon_storage databases"
# Mirrors .github/scripts/collect-setup-database.sh and tests/init-sql.sh:
# centreon/centreon and root_centreon/centreon users, then load the repository
# SQL dumps.
run "
  mkdir -p /run/mysqld && chown mysql:mysql /run/mysqld
  [ -d /var/lib/mysql/mysql ] || mariadb-install-db --user=mysql --datadir=/var/lib/mysql >/dev/null
  mariadbd --socket=/run/mysqld/mysqld.sock --user=root >/tmp/mariadb-boot.log 2>&1 &
  for i in \$(seq 1 60); do mysqladmin ping --silent 2>/dev/null && break; sleep 1; done

  mysql -e \"CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon'\"
  mysql -e \"CREATE USER IF NOT EXISTS 'root_centreon'@'localhost' IDENTIFIED BY 'centreon'\"
  mysql -e \"GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES,EVENT,CREATE VIEW ON *.* TO 'centreon'@'localhost'\"
  mysql -e \"GRANT ALL PRIVILEGES ON *.* TO 'root_centreon'@'localhost' WITH GRANT OPTION\"
  mysql -e \"FLUSH PRIVILEGES\"

  cd ${REPO_DIR}
  sed 's/DBNameConf/centreon/g' resources/centreon.sql > /tmp/centreon.sql
  mysql -u root_centreon -pcentreon < resources/centreon_storage.sql
  mysql -u root_centreon -pcentreon < /tmp/centreon.sql
  echo '--- databases available:'
  mysql -u root_centreon -pcentreon -e 'SHOW DATABASES' | grep -E 'centreon'
"

echo ">>> (5/6) uv and the robot venv (${TESTS_DIR}/robotframework)"
# The tests/README.md procedure, with the interpreter named explicitly.
#
# It must be the container's own /usr/bin/python3, and not a python uv downloads for
# itself: tests/ sits on the shared /work mount, so this venv is shared by every
# container working on this worktree -- while a venv hard-codes the absolute path of
# its interpreter. A uv-managed python lives under /root/.local/share/uv, which
# exists only in the container that installed it, and the venv then fails everywhere
# else with "cannot execute: required file not found". /usr/bin/python3 is at the
# same path in every container built from the same base.
#
# Left to itself, `uv venv` picks a python of its own and downloads it, which is how
# that happened. (The Python 3.11 pin this procedure used to carry came from a
# robotframework module since upgraded; that constraint is gone.)
run "
  export HOME=/root
  curl -LsSf https://astral.sh/uv/install.sh | sh
  export PATH=\"\$HOME/.local/bin:\$PATH\"
  uv --version
  cd ${TESTS_DIR}
  rm -rf robotframework
  uv venv --python=/usr/bin/python3 robotframework
  uv pip install --python robotframework/bin/python -U \
      robotframework \
      robotframework-databaselibrary \
      robotframework-examples pymysql \
      robotframework-requests psutil \
      robotframework-httpctrl boto3 \
      GitPython py-cpuinfo pyjwt \
      grpcio grpcio_tools \
      cython
"

echo ">>> (6/6) Generating the python gRPC stubs (init-proto.sh)"
# init-proto.sh compiles grpc_stream.proto, which imports the OpenTelemetry
# protos. Those live in <worktree>/opentelemetry-proto, a git-ignored directory
# that CMakeListsLinux.txt clones at build time -- so it is missing from a
# worktree that has never been built. Clone it here with the same command, to
# keep the container usable before any build has run.
run "
  export PATH=\"/root/.local/bin:\$PATH\"
  if [ ! -f ${REPO_DIR}/opentelemetry-proto/opentelemetry/proto/collector/metrics/v1/metrics_service.proto ]; then
    echo '--- cloning opentelemetry-proto (missing, as in a never-built worktree)'
    rm -rf ${REPO_DIR}/opentelemetry-proto
    git clone --depth=1 --single-branch \
      https://github.com/open-telemetry/opentelemetry-proto.git \
      ${REPO_DIR}/opentelemetry-proto
  fi
  cd ${TESTS_DIR}
  source robotframework/bin/activate
  ./init-proto.sh
"

cat <<EOF

======================================================================
Container '${CONTAINER}' is ready.

  Shell in:     podman exec -ti ${CONTAINER} /bin/bash

  Binaries:     the container holds NO centreon binary. Build on the host,
                then install inside the container:
                  (host)      ninja -C ${SRC}/${WORKTREE}/build
                  (container) cmake --install ${REPO_DIR}/build

  Robot tests:  cd ${TESTS_DIR}
                . robotframework/bin/activate
                robot -e unstable .

Note: MariaDB is running (socket /run/mysqld/mysqld.sock). After a
podman stop/start, restart the daemon with:
  mariadbd --socket=/run/mysqld/mysqld.sock --user=root &
and repair the databases if needed: cd ${TESTS_DIR} && ./repair-db.sh
======================================================================
EOF
