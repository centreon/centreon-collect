#!/bin/bash

# Tells if the Debian package $1 is installed. A tool is also accepted when its
# command is found in the PATH, since it may come from elsewhere than a Debian
# package (e.g. sccache installed with cargo).
is_installed() {
  local cmd
  case "$1" in
    ninja-build) cmd=ninja ;;
    cmake|curl|g++|gcc|git|mold|pkg-config|python3|sccache|zip) cmd="$1" ;;
    *) cmd= ;;
  esac
  if [ -n "$cmd" ] && command -v "$cmd" >/dev/null ; then
    return 0
  fi
  $dpkg -l "$1" | grep -q "^ii"
}

show_help() {
cat << EOF
Usage: ${0##*/} -n=[yes|no] -v

This program builds Centreon-broker, Centreon-engine, Centreon-agent and their
modules through cmake and vcpkg.

    -b|--build-dir : specify build directory
    -f|--force    : force rebuild
    -r|--release  : Build on release mode
    -og           : C++14 standard
    -clang        : Compilation with clang++
    -mold         : Link made with mold
    -legacy-mold  : Link made with mold but with an old gcc version
    -dr           : Debug robot enabled
    -sccache      : Compilation through sccache
    -h|--help     : help
EOF
}

BUILD_TYPE="Debug"

# Every path given to cmake is absolute: cmake stores them in its cache, and a
# relative one breaks as soon as cmake is re-run from another directory (e.g.
# by ninja from the build directory). The script can also be launched from
# anywhere.
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCAL_VCPKG="$SRC_DIR/vcpkg"
# vcpkg release this tree is built with.
VCPKG_TAG=2026.05.25

# Prints the vcpkg directory to use when VCPKG_ROOT is not defined: the local
# one if it exists, otherwise the one of another worktree of the repository if
# it is at the release VCPKG_TAG, otherwise the local one (to be cloned).
find_vcpkg() {
  local wt
  if [ -d "$LOCAL_VCPKG" ] ; then
    echo "$LOCAL_VCPKG"
    return
  fi
  while read -r wt ; do
    if [ "$wt" != "$SRC_DIR" ] && [ -d "$wt/vcpkg" ] &&
       [ "$(git -C "$wt/vcpkg" describe --tags --exact-match 2>/dev/null)" = "$VCPKG_TAG" ] ; then
      echo "$wt/vcpkg"
      return
    fi
  done < <(git -C "$SRC_DIR" worktree list --porcelain 2>/dev/null | sed -n 's/^worktree //p')
  echo "$LOCAL_VCPKG"
}

# An already defined VCPKG_ROOT is kept, only made absolute. A vcpkg borrowed
# from another worktree is shared like it.
export VCPKG_ROOT="$(realpath -m "${VCPKG_ROOT:-$(find_vcpkg)}")"
export PATH="$VCPKG_ROOT:$PATH"
echo "vcpkg used: $VCPKG_ROOT"
BUILD_DIR="$SRC_DIR/build"
force=0

COMPILER=gcc
CC=gcc
CXX=g++
DR=
SC=0
MOLD=

TRIPLET=x64-linux
# Options not handled here are given to cmake.
CMAKE_ARGS=()

while [ $# -gt 0 ]
do
  case "$1" in
    -f|--force)
      echo "Forced rebuild"
      force=1
      ;;
    -b|--build-dir)
      echo "Build directory specified: $2"
      # Absolute, as every path given to cmake.
      BUILD_DIR="$(realpath -m "$2")"
      shift
      ;;
    -og)
      echo "C++14 applied on this compilation"
      STD="gnu14"
      ;;
    -dr|--debug-robot)
      echo "DEBUG_ROBOT enabled"
      DR="-DDEBUG_ROBOT=ON"
      ;;
    -r|--release)
      echo "Release build"
      BUILD_TYPE="RelWithDebInfo"
      ;;
    -clang)
      COMPILER=clang
      CC=clang
      CXX=clang++
      # The vcpkg dependencies must be built with clang too.
      TRIPLET=x64-linux-clang
      ;;
    -mold)
      MOLD=-fuse-ld=mold
      ;;
    -legacy-mold)
      MOLD="-B /usr/bin/mold"
      ;;
    -sccache)
      SC="1"
      ;;
    -h|--help)
      show_help
      exit 2
      ;;
    *)
      CMAKE_ARGS+=("$1")
      ;;
  esac
  shift
done

# Am I root?
my_id=$(id -u)

if [ -r /etc/centos-release -o -r /etc/almalinux-release ] ; then
  if [ -r /etc/almalinux-release ] ; then
    maj="centos$(cat /etc/almalinux-release | awk '{print $3}' | cut -f1 -d'.')"
  else
    maj="centos$(cat /etc/centos-release | awk '{print $4}' | cut -f1 -d'.')"
  fi
  v=$(cmake --version)
  if [[ "$v" =~ "version 3" ]] ; then
    cmake='cmake'
  else
    if rpm -q cmake3 ; then
      cmake='cmake3'
    elif [[ "$maj" == "centos7" ]] ; then
      yum -y install epel-release cmake3
      cmake='cmake3'
    elif [[ "$maj" == "centos9" ]] ; then
      dnf config-manager --set-enabled crb
      dnf -y install epel-release
    else
      dnf -y install cmake
      cmake='cmake'
    fi
  fi
  yum -y install gcc-c++
  if [[ ! -x /usr/bin/python3 ]] ; then
    yum -y install python3
  else
    echo "python3 already installed"
  fi
  if ! rpm -q python3-pip ; then
    yum -y install python3-pip
  else
    echo "pip3 already installed"
  fi

  good=$(gcc --version | awk '/gcc/ && ($3+0)>5.0{print 1}')

  if [ ! $good ] ; then
    yum -y install centos-release-scl
    yum-config-manager --enable rhel-server-rhscl-7-rpms
    yum -y install devtoolset-9
    ln -s /usr/bin/cmake3 /usr/bin/cmake
    source /opt/rh/devtoolset-9/enable
  fi

  pkgs=(
    gcc-c++
    curl
    ninja-build
    rrdtool-devel
    gnutls-devel
    lua-devel
    perl-Thread-Queue
    perl-devel
    perl-ExtUtils-Embed
    perl-srpm-macros
    libgcrypt-devel
    openssl-devel
    libssh2-devel
    libcurl-devel
    tar
    zlib-devel
    libstdc++-static
  )
  if [[ "$maj" == 'centos8' ]] ; then
    dnf config-manager --set-enabled powertools
    dnf update
  fi

  for i in "${pkgs[@]}"; do
    if ! rpm -q $i ; then
      if [[ "$maj" == 'centos7' ]] ; then
        yum install -y $i
      elif [[ "$maj" == 'centos8' ]] ; then
        yum install -y $i
      else
        dnf -y install $i
      fi
    fi
  done
elif [ -r /etc/issue ] ; then
  maj=$(head -1 /etc/issue | awk '{print $1}')
  version=$(head -1 /etc/issue | awk '{print $3}')
  if [[ "$version" == "9" ]] ; then
    dpkg="dpkg"
  else
    dpkg="dpkg --no-pager"
  fi
  v=$(cmake --version)
  if [[ "$v" =~ "version 3" ]] ; then
    cmake='cmake'
  elif [[ "$maj" == "Debian" ]] || [[ "$maj" == "Ubuntu" ]]; then
    if $dpkg -l cmake ; then
      echo "Bad version of cmake..."
      exit 1
    else
      echo -e "cmake is not installed, you could enter, as root:\n\tapt install -y cmake\n\n"
      cmake='cmake'
    fi
  elif [[ "$maj" == "Raspbian" ]] ; then
    if $dpkg -l cmake ; then
      echo "Bad version of cmake..."
      exit 1
    else
      echo -e "cmake is not installed, you could enter, as root:\n\tapt install -y cmake\n\n"
      cmake='cmake'
    fi
  else
    echo "Bad version of cmake..."
    exit 1
  fi

  if [[ "$maj" == "Debian" ]] || [[ "$maj" == "Ubuntu" ]]; then
    if [[ "$maj" == "Debian" ]] && ([[ "$version" == "13" ]] || [[ "$version" =~ "trixie" ]]); then
      lua_pkg="liblua5.4-dev"
    else
      lua_pkg="liblua5.3-dev"
    fi
    pkgs=(
      cmake
      g++
      gcc
      curl
      git
      libcurl4-openssl-dev
      libgcrypt20-dev
      libgnutls28-dev
      $lua_pkg
      libmariadb-dev
      libperl-dev
      libhttp-daemon-ssl-perl
      libjson-perl
      librrd-dev
      libssh2-1-dev
      libssl-dev
      mold
      ninja-build
      pkg-config
      python3
      python3-pip
      rrdtool
      zip
      zlib1g-dev
    )
    apt update
    for i in "${pkgs[@]}"; do
      if ! is_installed "$i" ; then
        if [[ "$my_id" == 0 ]] ; then
          apt install -y $i
        else
          echo -e "The package \"$i\" is not installed, you can install it, as root, with the command:\n\tapt install -y $i\n\n"
          exit 1
        fi
      fi
    done
  elif [[ "$maj" == "Raspbian" ]] ; then
    pkgs=(
      gcc
      g++
      curl
      pkg-config
      libmariadb3
      librrd-dev
      libgnutls28-dev
      ninja-build
      liblua5.3-dev
      python3
      python3-pip
      libperl-dev
      libgcrypt20-dev
      libssl-dev
      libssh2-1-dev
      zlib1g-dev
    )
    for i in "${pkgs[@]}"; do
      if ! is_installed "$i" ; then
        if [[ "$my_id" == 0 ]] ; then
          apt install -y $i
        else
          echo -e "The package \"$i\" is not installed, you can install it, as root, with the command:\n\tapt install -y $i\n\n"
          exit 1
        fi
      fi
    done
  fi
fi

if [ "$force" = "1" ] ; then
  echo "Build forced, removing the '$BUILD_DIR' directory before recreating it"
  rm -rf "$BUILD_DIR"
  # A vcpkg shared through VCPKG_ROOT belongs to other trees too: only the
  # local one is removed.
  if [ "$VCPKG_ROOT" = "$LOCAL_VCPKG" ] ; then
    rm -rf "$LOCAL_VCPKG"
  else
    echo "Shared vcpkg '$VCPKG_ROOT' kept"
  fi
  mkdir "$BUILD_DIR"
  ln -s "$SRC_DIR/overlays" "$BUILD_DIR/overlays"
fi

if [ ! -d "$VCPKG_ROOT" ] ; then
  echo "No vcpkg directory. Cloning the repo into '$VCPKG_ROOT'"
  git clone -b "$VCPKG_TAG" https://github.com/Microsoft/vcpkg.git "$VCPKG_ROOT"
  "$VCPKG_ROOT/bootstrap-vcpkg.sh"
fi

case "$COMPILER" in
clang)
  VERSION=$(clang -dumpversion | awk -F '.' '{print $1}')
  ;;
gcc)
  VERSION=$(gcc -dumpversion)
  ;;
esac

if [ "$SC" -eq 1 ] ; then
  SCCACHE="-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache"
else
  SCCACHE=
fi

if [[ "$maj" == "Raspbian" ]] ; then
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra $MOLD" $cmake -B "$BUILD_DIR" -DVCPKG_OVERLAY_TRIPLETS="$SRC_DIR/custom-triplets" -DVCPKG_TARGET_TRIPLET=$TRIPLET -DVCPKG_OVERLAY_PORTS="$SRC_DIR/overlays" -GNinja $DR $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_CREATE_FILES=OFF $NG "${CMAKE_ARGS[@]}" -S "$SRC_DIR"

elif [[ "$maj" == "Debian" ]] ; then
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra $MOLD" $cmake -B "$BUILD_DIR" -DVCPKG_OVERLAY_TRIPLETS="$SRC_DIR/custom-triplets" -DVCPKG_TARGET_TRIPLET=$TRIPLET -DVCPKG_OVERLAY_PORTS="$SRC_DIR/overlays" -GNinja $DR $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_CREATE_FILES=OFF $NG "${CMAKE_ARGS[@]}" -S "$SRC_DIR"
else
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra $MOLD" $cmake -B "$BUILD_DIR" -DVCPKG_OVERLAY_TRIPLETS="$SRC_DIR/custom-triplets" -DVCPKG_TARGET_TRIPLET=$TRIPLET -DVCPKG_OVERLAY_PORTS="$SRC_DIR/overlays" -S "$SRC_DIR" -GNinja $DR $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_CREATE_FILES=OFF -DWITH_CONF=OFF "${CMAKE_ARGS[@]}"
fi
