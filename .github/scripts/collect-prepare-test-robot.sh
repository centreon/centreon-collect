#!/bin/bash
set -e
set -x


database_type=$1

. /etc/os-release
distrib=${ID}
distrib=$(echo $distrib | tr '[:lower:]' '[:upper:]')


echo "########################### Configure sshd ###########################"
ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key -P "" <<<y
ssh-keygen -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key -P "" <<<y
ssh-keygen -t ed25519 -f /etc/ssh/ssh_host_ed25519_key -P "" <<<y
echo >> /etc/ssh/sshd_config
#rsa are not allowed by default in recent version of sshd
echo "PubkeyAcceptedAlgorithms +ssh-rsa" >> /etc/ssh/sshd_config
echo "HostKeyAlgorithms +ssh-rsa" >> /etc/ssh/sshd_config
echo "PubkeyAcceptedKeyTypes +ssh-rsa" >> /etc/ssh/sshd_config

mkdir -p /run/sshd

.github/scripts/collect-setup-database.sh $database_type

if [ $database_type == 'mysql' ]; then
  killall -w mysqldtoto
else
  killall -w mariadbd
fi

if [ "$distrib" = "ALMALINUX" ]; then
  dnf groupinstall -y "Development Tools"
  dnf install -y python3-devel
  dnf clean all
else
  apt-get update
  apt-get install -y build-essential
  apt-get install -y python3-dev
  apt-get clean
fi
