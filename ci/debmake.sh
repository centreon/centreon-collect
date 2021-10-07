#!/bin/sh

CUR_VERSION="21.10.0"
AUTHOR="Luiz Costa"
AUTHOR_EMAIL="me@luizgustavo.pro.br"

if ! [ -d ./repo ]; then
    mkdir -p ./repo
    cd ./repo
    wget -q -O - https://github.com/centreon/centreon-collect/archive/refs/heads/develop.tar.gz | tar zxpf -
    mv centreon-collect-develop centreon-collect-${CUR_VERSION}
    tar czpvf centreon-collect-${CUR_VERSION}.tar.gz centreon-collect-${CUR_VERSION}
    cp -rv ../debian centreon-collect-${CUR_VERSION}
    cd centreon-collect-${CUR_VERSION}
    debmake -f "${AUTHOR}" -e "${AUTHOR_EMAIL}"
    debuild-pbuilder
    cd ../..
    rm -rf ./repo
fi