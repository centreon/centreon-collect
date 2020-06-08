#!/bin/bash

RESET='\033[0m'
RED='\033[0;31m'
LGREEN='\033[0;32m'

if [[ $1 == "-f" ]] ; then
  force="rm"
elif [[ $1 == "-n" ]] ; then
  force="ignore"
fi

root_dir=$PWD
if [[ $(basename $root_dir) != "benchmark" ]] ; then
  echo "This script must be executed from the benchmark directory"
  exit 1
fi

if [[ -d log ]] ; then
  rm -rf log
fi
mkdir log

if [[ -d lib ]] ; then
  rm -rf lib
fi
mkdir -p lib/rw

if [[ -d "build" ]] ; then
  a=""
  if [[ $force == "rm" ]] ; then
    a="y"
  elif [[ $force == "ignore" ]] ; then
    a="n"
  fi
  while [[ $a != "y" && $a != "n" ]] ; do
    echo "Do you want to clean the build directory?"
    read a
  done
  if [[ $a == "y" ]] ; then
    rm -rf build
    mkdir build
  fi
else
  mkdir build
fi

conan=$(command -v conan)
if [[ $? -ne 0 ]]; then
  if [[ -x $HOME/.local/bin/conan ]] ; then
    echo "conan installed in a local path"
    conan="python3 $HOME/.local/bin/conan"
  else
    echo "Conan not installed"
    echo "You can install it with your package manager or with pip"
    exit 1
  fi
fi

cd build || exit 3
$conan remote add centreon https://api.bintray.com/conan/centreon/centreon

if [[ -r /usr/share/centreon ]] ; then
  $conan install --remote centreon ../..
else
  $conan install --build missing ../..
fi

if [[ $? -ne 0 ]] ; then
  echo "Error during conan packages installation..."
  exit 1
fi

cmake=$(command -v cmake3)
if [[ $? -ne 0 ]] ; then
  cmake=$(command -v cmake)
fi

$cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$root_dir/app ../..

if [[ $? -ne 0 ]] ; then
  echo "Error..."
  exit 2
fi

echo -e "\n${LGREEN}Building Engine${RESET}"
ninja
ninja install

cd $root_dir || exit 4
echo -e "\n${LGREEN}Building Config${RESET}"
python3 ./build_conf.py conf.json

echo -e "\n${LGREEN}Building Config${RESET}"
if ! protoc --plugin=protoc-gen-grpc=/usr/bin/grpc_python_plugin --proto_path=.. --grpc_out=. ../enginerpc/engine.proto ; then
  echo "Error: protoc for grpc failed..."
fi

echo -e "\n${LGREEN}Launching Mariadb${RESET}"
docker run --rm -p 3306:3306 -d centreon_storage

echo -e "\n${LGREEN}Launching Engine${RESET}"
LD_LIBRARY_PATH=app/lib:$LD_LIBRARY_PATH app/sbin/centengine centreon-engine/centengine.cfg
