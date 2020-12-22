#!/bin/bash

if [ "$1" = "-f" ] ; then
  force=1
  shift
fi

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
      yum -y install epel-release
      yum -y install cmake3
      cmake='cmake3'
    else
      dnf -y install cmake
      cmake='cmake'
    fi
  fi

  if ! rpm -q gcc-c++ ; then
    yum -y install gcc-c++
  fi

  pkgs=(
    ninja-build
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
  v=$(cmake --version)
  if [[ $v =~ "version 3" ]] ; then
    cmake='cmake'
  elif [ $maj = "Debian" ] ; then
    if [ $version = "9" ] ; then
      dpkg="dpkg"
    else
      dpkg="dpkg --no-pager"
    fi
    if $dpkg -l --no-pager cmake ; then
      echo "Bad version of cmake..."
      exit 1
    else
      if [ $my_id -eq 0 ] ; then
        apt install -y cmake
        cmake='cmake'
      else
        echo -e "cmake is not installed, you could enter, as root:\n\tapt install -y cmake\n\n"
        exit 1
      fi
    fi
    pkgs=(
      gcc
      g++
      ninja-build
      pkg-config
    )
    for i in "${pkgs[@]}"; do
      if ! $dpkg -l --no-pager $i | grep "^ii" ; then
        if [ $my_id -eq 0 ] ; then
          apt install -y $i
        else
          echo -e "The package \"$i\" is not installed, you can install it, as root, with the command:\n\tapt install -y $i\n\n"
          exit 1
        fi
      fi
    done
  else
    echo "Bad version of cmake..."
    exit 1
  fi
fi

if [ ! -d build ] ; then
  mkdir build
fi

cd build
CXXFLAGS="-Wall -Wextra" $cmake -DWITH_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX_LIB=/usr/lib64 -DWITH_TESTING=On $* ..

#CXX=/usr/bin/clang++ CC=/usr/bin/clang CXXFLAGS="-Wall -Wextra" cmake -DWITH_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX_LIB=/usr/lib64 -DWITH_TESTING=On $* ..

#CXXFLAGS="-Wall -Wextra -O1 -fsanitize=address -fno-omit-frame-pointer" cmake -DWITH_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX_LIB=/usr/lib64 -DWITH_TESTING=On $* ..
