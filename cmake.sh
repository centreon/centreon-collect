#!/bin/bash

show_help() {
cat << EOF
Usage: ${0##*/} -n=[yes|no] -v

This program build Centreon-broker

    -f|--force    : force rebuild
    -r|--release  : Build on release mode
    -fcr|--force-conan-rebuild : rebuild conan data
    -ng           : C++17 standard
    -h|--help     : help
EOF
}
BUILD_TYPE="Debug"
CONAN_REBUILD="0"
for i in $(cat conanfile.txt) ; do
  if [[ "$i" =~ / ]] ; then
    if [ ! -d ~/.conan/data/$i ] ; then
      echo "The package '$i' is missing"
      CONAN_REBUILD="1"
      break
    fi
  fi
done

STD=14

for i in "$@"
do
  case "$i" in
    -f|--force)
      echo "Forced rebuild"
      force=1
      shift
      ;;
    -ng)
      echo "C++17 applied on this compilation"
      STD="17"
      shift
      ;;
    -r|--release)
      echo "Release build"
      BUILD_TYPE="Release"
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
  if [ -r /etc/centos-release ] ; then
    maj="centos$(cat /etc/centos-release | awk '{print $4}' | cut -f1 -d'.')"
  else
    maj="centos$(cat /etc/almalinux-release | awk '{print $3}' | cut -f1 -d'.')"
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
  if [[ "$maj" == "centos7" ]] ; then
    if [[ ! -x /opt/rh/rh-python38 ]] ; then
      yum -y install centos-release-scl
      yum -y install rh-python38
      source /opt/rh/rh-python38/enable
    else
      echo "python38 already installed"
    fi
  else
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
  )
  for i in "${pkgs[@]}"; do
    if ! rpm -q $i ; then
      if [[ "$maj" == 'centos7' ]] ; then
        yum install -y $i
      elif [[ "$maj" == 'centos8' ]] ; then
        dnf -y --enablerepo=PowerTools install $i
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

pip3 install conan --upgrade

if which conan ; then
  conan=$(which conan)
elif [[ -x "/usr/local/bin/conan" ]] ; then
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
  rm -rf build
  mkdir build
fi
cd build
if [[ "$maj" == "centos7" ]] ; then
  rm -rf ~/.conan/profiles/default
  if [[ "$CONAN_REBUILD" == "1" ]] ; then
    $conan install .. -s compiler.cppstd=$STD -s compiler.libcxx=libstdc++11 --build="*"
  else
    $conan install .. -s compiler.cppstd=$STD -s compiler.libcxx=libstdc++11 --build=missing
  fi
else
    $conan install .. -s compiler.cppstd=$STD -s compiler.libcxx=libstdc++11 --build=missing
fi

if [[ $STD -eq 17 ]] ; then
  NG="-DNG=ON"
else
  NG="-DNG=OFF"
fi

if [[ "$maj" == "Raspbian" ]] ; then
  CXXFLAGS="-Wall -Wextra" $cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF $NG $* ..
elif [[ "$maj" == "Debian" ]] ; then
  CXXFLAGS="-Wall -Wextra" $cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF $NG $* ..
else
  CXXFLAGS="-Wall -Wextra" $cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF -DWITH_CONF=OFF $NG $* ..
fi
