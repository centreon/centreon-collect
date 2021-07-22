#!/bin/bash

show_help() {
cat << EOF
Usage: ${0##*/} -n=[yes|no] -v

This program build Centreon-broker

    -f|--force    : force rebuild
    -r|--release  : Build on release mode
    -fcr|--force-conan-rebuild : rebuild conan data
    -h|--help     : help
EOF
}
BUILD_TYPE="Debug"
CONAN_REBUILD="0"
for i in $(cat conanfile.txt) ; do
  if [[ $i =~ / ]] ; then
    if [ ! -d ~/.conan/data/$i ] ; then
      echo "The package '$i' is missing"
      CONAN_REBUILD="1"
      break
    fi
  fi
done

for i in "$@"
do
  case $i in
    -f|--force)
      force=1
      shift
      ;;
    -r|--release)
      BUILD_TYPE="Release"
      shift
      ;;
    -fcr|--force-conan-rebuild)
      CONAN_REBUILD="1"
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

if [ -r /etc/centos-release ] ; then
  maj="centos$(cat /etc/centos-release | awk '{print $4}' | cut -f1 -d'.')"
  v=$(cmake --version)
  if [[ $v =~ "version 3" ]] ; then
    cmake='cmake'
  else
    if rpm -q cmake3 ; then
      cmake='cmake3'
    elif [ $maj = "centos7" ] ; then
      yum -y install epel-release cmake3
      cmake='cmake3'
    else
      dnf -y install cmake
      cmake='cmake'
    fi
  fi
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
    perl-Thread-Queue
    perl-devel
    libssh2-devel
    libgcrypt-devel
  )
  for i in "${pkgs[@]}"; do
    if ! rpm -q $i ; then
      if [ $maj = 'centos7' ] ; then
        yum install -y $i
      else
        dnf -y --enablerepo=PowerTools install $i
      fi
    fi
  done
elif [ -r /etc/issue ] ; then
  maj=$(cat /etc/issue | awk '{print $1}')
  version=$(cat /etc/issue | awk '{print $3}')
  if [ $version = "9" ] ; then
    dpkg="dpkg"
  else
    dpkg="dpkg --no-pager"
  fi
  v=$(cmake --version)
  if [[ $v =~ "version 3" ]] ; then
    cmake='cmake'
  elif [ $maj = "Debian" ] ; then
    if $dpkg -l cmake ; then
      echo "Bad version of cmake..."
      exit 1
    else
      echo -e "cmake is not installed, you could enter, as root:\n\tapt install -y cmake\n\n"
      cmake='cmake'
    fi
  elif [ $maj = "Raspbian" ] ; then
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

  if [ $maj = "Debian" ] ; then
    pkgs=(
      python3
      python3-pip
      libperl-dev
      libssh2-1-dev
      libgcrypt20-dev
    )
    for i in "${pkgs[@]}"; do
      if ! $dpkg -l $i | grep "^ii" ; then
        if [ $my_id -eq 0 ] ; then
          apt install -y $i
        else
          echo -e "The package \"$i\" is not installed, you can install it, as root, with the command:\n\tapt install -y $i\n\n"
          exit 1
        fi
      fi
    done
  elif [ $maj = "Raspbian" ] ; then
    pkgs=(
      python3
      python3-pip
      libperl-dev
      libssh2-1-dev
      libgcrypt20-dev
    )
    for i in "${pkgs[@]}"; do
      if ! $dpkg -l $i | grep "^ii" ; then
        if [ $my_id -eq 0 ] ; then
          apt install -y $i
        else
          echo -e "The package \"$i\" is not installed, you can install it, as root, with the command:\n\tapt install -y $i\n\n"
          exit 1
        fi
      fi
    done
  fi
  if [[ ! -x /usr/bin/python3 ]] ; then
    if [ $my_id -eq 0 ] ; then
      apt install -y python3
    else
      echo -e "python3 is not installed, you can enter, as root:\n\tapt install -y python3\n\n"
      exit 1
    fi
  else
    echo "python3 already installed"
  fi
  if ! $dpkg -l python3-pip ; then
    if [ $my_id -eq 0 ] ; then
      apt install -y python3-pip
    else
      echo -e "python3-pip is not installed, you can enter, as root:\n\tapt install -y python3-pip\n\n"
      exit 1
    fi
  else
    echo "pip3 already installed"
  fi
fi

pip3 install conan --upgrade

if [ $my_id -eq 0 ] ; then
  conan='/usr/local/bin/conan'
elif which conan ; then
  conan=$(which conan)
else
  conan="$HOME/.local/bin/conan"
fi

if [ ! -d build ] ; then
  mkdir build
else
  echo "'build' directory already there"
fi

if [ "$force" = "1" ] ; then
  rm -rf build
  mkdir build
fi
cd build
if [ $maj = "centos7" ] ; then
  rm -rf ~/.conan/profiles/default
  if [ "$CONAN_REBUILD" = "1" ] ; then
    $conan install .. -s compiler.libcxx=libstdc++11 --build="*"
  else
    $conan install .. -s compiler.libcxx=libstdc++11 --build=missing
  fi
else
    $conan install .. -s compiler.libcxx=libstdc++11 --build=missing
fi

CXXFLAGS="-Wall -Wextra" "$cmake" -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTING=On -DWITH_PREFIX_BINARY=/usr/lib64/centreon-connector/ $* ..

#CXX=/usr/bin/clang++ CC=/usr/bin/clang cmake -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER=centreon-engine -DWITH_GROUP=centreon-engine -DCMAKE_BUILD_TYPE=Debug -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_PREFIX_CONF=/etc/centreon-engine -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_PREFIX_LIB=/usr/lib64/centreon-engine -DWITH_TESTING=On -DWITH_SIMU=On .

#CXX=/usr/lib64/ccache/g++ CC=/usr/lib64/ccache/gcc cmake -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER=centreon-engine -DWITH_GROUP=centreon-engine -DCMAKE_BUILD_TYPE=Debug -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_PREFIX_CONF=/etc/centreon-engine -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_PREFIX_LIB=/usr/lib64/centreon-engine -DWITH_TESTING=On -DWITH_SIMU=On .

#CXXFLAGS="-O1 -fsanitize=address -fno-omit-frame-pointer" cmake -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER=centreon-engine -DWITH_GROUP=centreon-engine -DCMAKE_BUILD_TYPE=Debug -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_PREFIX_CONF=/etc/centreon-engine -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_PREFIX_LIB=/usr/lib64/centreon-engine -DWITH_TESTING=On -DWITH_SIMU=On .
