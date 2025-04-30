#!/bin/bash

show_help() {
cat << EOF
Usage: ${0##*/} -n=[yes|no] -v

This program build Centreon-broker

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

export VCPKG_ROOT=$PWD/vcpkg
export PATH=$VCPKG_ROOT:$PATH
echo "Please, before the build, execute the following commands:"
echo "export VCPKG_ROOT=\$PWD/vcpkg"
echo "export PATH=\$VCPKG_ROOT:\$PATH"

COMPILER=gcc
CC=gcc
CXX=g++
WITH_CLANG=OFF
DR=
SC=0
MOLD=

for i in "$@"
do
  case "$i" in
    -f|--force)
      echo "Forced rebuild"
      force=1
      shift
      ;;
    -og)
      echo "C++14 applied on this compilation"
      STD="gnu14"
      shift
      ;;
    -dr|--debug-robot)
      echo "DEBUG_ROBOT enabled"
      DR="-DDEBUG_ROBOT=ON"
      shift
      ;;
    -r|--release)
      echo "Release build"
      BUILD_TYPE="RelWithDebInfo"
      shift
      ;;
    -clang)
      COMPILER=clang
      WITH_CLANG=ON
      CC=clang
      CXX=clang++
      shift
      ;;
    -mold)
      MOLD=-fuse-ld=mold
      shift
      ;;
    -legacy-mold)
      MOLD="-B /usr/bin/mold"
      shift
      ;;
    -sccache)
      SC="1"
      shift
      ;;
    -h|--help)
      show_help
      exit 2
      ;;
    *)
            # unknown option
    ;;
  esac
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
    pkgs=(
      cmake
      g++
      gcc
      libcurl4-openssl-dev
      libgcrypt20-dev
      libgnutls28-dev
      liblua5.3-dev
      libmariadb-dev
      libperl-dev
      librrd-dev
      libssh2-1-dev
      libssl-dev
      ninja-build
      pkg-config
      python3
      python3-pip
      zip
      zlib1g-dev
    )
    for i in "${pkgs[@]}"; do
      if ! $dpkg -l $i | grep "^ii" ; then
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
      if ! $dpkg -l $i | grep "^ii" ; then
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

if [ ! -d vcpkg ] ; then
  echo "No vcpkg directory. Cloning the repo"
  git clone --depth 1 https://github.com/Microsoft/vcpkg.git
  ./vcpkg/bootstrap-vcpkg.sh
fi

if [ "$force" = "1" ] ; then
  echo "Build forced, removing the 'build' directory before recreating it"
  rm -rf build
  mkdir build
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
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra $MOLD" $cmake $DR -DWITH_CLANG=$WITH_CLANG $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF $NG $* ..
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra $MOLD" $cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux-release -DVCPKG_OVERLAY_PORTS=overlays -GNinja $DR -DWITH_CLANG=$WITH_CLANG $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF $NG $* -S .

elif [[ "$maj" == "Debian" ]] ; then
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra $MOLD" $cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux-release -DVCPKG_OVERLAY_PORTS=overlays -GNinja $DR -DWITH_CLANG=$WITH_CLANG $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF $NG $* -S .
else
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra $MOLD" $cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux-release -DVCPKG_OVERLAY_PORTS=overlays -S . -GNinja $DR -DWITH_CLANG=$WITH_CLANG $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF -DWITH_CONF=OFF $*
fi
