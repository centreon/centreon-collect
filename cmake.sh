#!/bin/bash

show_help() {
cat << EOF
Usage: ${0##*/} -n=[yes|no] -v

This program build Centreon-broker

    -f|--force    : force rebuild
    -r|--release  : Build on release mode
    -fcr|--force-conan-rebuild : rebuild conan data
    -clang        : Compilation with clang++
    -dr           : Debug robot enabled
    -sccache      : Compilation through sccache
    -h|--help     : help
EOF
}
BUILD_TYPE="Debug"
CONAN_REBUILD="0"

COMPILER=gcc
CC=gcc
CXX=g++
WITH_CLANG=OFF
EE=
DR=
SC=0

for i in "$@"
do
  case "$i" in
    -f|--force)
      echo "Forced rebuild"
      force=1
      shift
      ;;
    -dr|--debug-robot)
      echo "DEBUG_ROBOT enabled"
      DR="-DDEBUG_ROBOT=ON"
      shift
      ;;
    -r|--release)
      echo "Release build"
      BUILD_TYPE="Release"
      shift
      ;;
    -clang)
      COMPILER=clang
      WITH_CLANG=ON
      EE="-e CXX=/usr/bin/clang++ -e CC=/usr/bin/clang -e:b CXX=/usr/bin/clang++ -e:b CC=/usr/bin/clang"
      CC=clang
      CXX=clang++
      shift
      ;;
    -sccache)
      SC="1"
      shift
      ;;
    -fcr|--force-conan-rebuild)
      echo "Forced conan rebuild"
      CONAN_REBUILD="1"
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
    else
      dnf -y install cmake
      cmake='cmake'
    fi
  fi
  yum -y install gcc-c++
  if [[ ! -x /usr/bin/python3.9 ]] ; then
    yum -y install python39-devel
    rm /etc/alternatives/python3
    ln -s /usr/bin/python3.9 /etc/alternatives/python3
  else
    echo "python3 already installed"
  fi
  if ! rpm -q python39-pip ; then
    yum -y install python39-pip
    rm /etc/alternatives/pip3
    ln -s /usr/bin/pip3.9 /etc/alternatives/pip3
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
    mariadb-connector-c-devel
    openssl-devel
    protobuf-c-devel
    grpc-devel
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
  if ! which cmake ; then
    apt install -y cmake
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
      gcc
      g++
      pkg-config
      librrd-dev
      libgnutls28-dev
      ninja-build
      liblua5.3-dev
      python3
      python3-pip
      libperl-dev
      libgcrypt20-dev
      libssl-dev
      libprotobuf-dev
      protobuf-compiler
      libgrpc++-dev
      protobuf-compiler-grpc
      libmariadb-dev
      libcurl4-gnutls-dev
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
      g++
      gcc
      libcurl4-gnutls-dev
      libgcrypt20-dev
      libgnutls28-dev
      libgrpc++-dev
      liblua5.3-dev
      libmariadb-dev
      libmariadb3
      libperl-dev
      libprotobuf-dev
      librrd-dev
      libssh2-1-dev
      libssl-dev
      ninja-build
      pkg-config
      protobuf-compiler
      protobuf-compiler-grpc
      python3
      python3-pip
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
  if [[ ! -x /usr/bin/python3 ]] ; then
    if [[ "$my_id" == 0 ]] ; then
      apt install -y python3
    else
      echo -e "python3 is not installed, you can enter, as root:\n\tapt install -y python3\n\n"
      exit 1
    fi
  else
    echo "python3 already installed"
  fi
  if ! $dpkg -l python3-pip ; then
    if [[ "$my_id" == 0 ]] ; then
      apt install -y python3-pip
    else
      echo -e "python3-pip is not installed, you can enter, as root:\n\tapt install -y python3-pip\n\n"
      exit 1
    fi
  else
    echo "pip3 already installed"
  fi
fi

if ! pip3 install conan --upgrade --break-system-packages ; then
  pip3 install conan --upgrade
fi

if which conan ; then
  conan=$(which conan)
elif [[ -x /usr/local/bin/conan ]] ; then
  conan='/usr/local/bin/conan'
else
  conan="$HOME/.local/bin/conan"
fi

if [ ! -d build ] ; then
  mkdir build
else
  echo "'build' directory already there"
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

if [ "$CONAN_REBUILD" -eq 1 -o ! -r "$HOME/.conan2/profiles/default" ] ; then
  echo "Creating default profile"
  mkdir -p "$HOME/.conan2/profiles"
  cat << EOF > "$HOME/.conan2/profiles/default"
[settings]
arch=x86_64
build_type=Release
compiler=${COMPILER}
compiler.cppstd=${STD}
compiler.libcxx=libstdc++11
compiler.version=$VERSION
os=Linux
EOF
fi

echo "$conan install .. --build=missing"
$conan install . --output-folder=build --build=missing

if [ "$SC" -eq 1 ] ; then
  SCCACHE="-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache"
else
  SCCACHE=
fi

if [[ "$maj" == "Raspbian" || "$maj" == "Debian" ]] ; then
  v=$($cmake --version)
  echo "##### CMake version ${v} #####"
  regex="version 3\.([0-9]*)"
  if [[ "$v" =~ $regex ]] ; then
    min=${BASH_REMATCH[1]}
    if ((min < 23)) ; then
      echo "deb http://deb.debian.org/debian bullseye-backports main contrib non-free" >> /etc/apt/sources.list
      apt update
      apt install -y cmake/bullseye-backports
    fi
  fi
fi

cd build

if [[ "$maj" == "Raspbian" ]] ; then
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra -std=gnu++17" $cmake --preset conan-release $DR -DWITH_CLANG=$WITH_CLANG $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF $* ..
elif [[ "$maj" == "Debian" ]] ; then
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra -std=gnu++17" $cmake --preset conan-release $DR -DWITH_CLANG=$WITH_CLANG $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF -DWITH_CONF=OFF $* ..
else
  CC=$CC CXX=$CXX CXXFLAGS="-Wall -Wextra -std=gnu++17" $cmake --preset conan-release $DR -DWITH_CLANG=$WITH_CLANG $SCCACHE -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF -DWITH_CONF=OFF $* ..
fi
