#!/bin/bash
#set -e
set -x

export RUN_ENV=docker

test_file=$1
database_type=$2
#this env variable is a json that contains some test params
export TESTS_PARAMS='$3'

. /etc/os-release
distrib=${ID}
distrib=$(echo $distrib | tr '[:lower:]' '[:upper:]')

#cpu=$(lscpu | awk '$1 ~ "Architecture" { print $2 }')
if [[ "$test_file" =~ "unstable" ]] ; then
  exit 0
fi

if [ ${database_type} == 'mysql' ] && [ ! -f tests/${test_file}.mysql ]; then
    echo > tests/log.html
    echo '<?xml version="1.0" encoding="UTF-8"?>' > tests/output.xml
    echo '<robot generator="Robot 6.0.2 (Python 3.9.14 on linux)" generated="20230517 15:35:12.235" rpa="false" schemaversion="3"></robot>' >> tests/output.xml
    echo > tests/report.html
    exit 0
fi

echo "###########################  start sshd ###########################"
if [ ! -f /etc/ssh/ssh_host_rsa_key ]; then
  ssh-keygen -q -t rsa -N '' -f /etc/ssh/ssh_host_rsa_key
  ssh-keygen -q -t dsa -N '' -f /etc/ssh/ssh_host_dsa_key
fi

grep -v pam_se /etc/pam.d/sshd > /tmp/sshd
mv /tmp/sshd /etc/pam.d/sshd


/usr/sbin/sshd -D -ddd -E /tmp/sshd.log 2>>/tmp/sshd.log &

ssh-keygen -q -t rsa -N '' -f ~/.ssh/id_rsa

for ip_end in `echo "1 2 3 4 5 6 7 8 9 10 11 12"`; do
  ip="127.0.0.$ip_end"
  echo "Adding $ip to known_hosts"
  echo -n "$ip " >> ~/.ssh/known_hosts
  cat /etc/ssh/ssh_host_rsa_key.pub >> ~/.ssh/known_hosts
done



echo "########################### algos sshd ###########################"
/usr/sbin/sshd -T | grep -E 'ciphers|macs|kexalgorithms'

# echo >> /etc/ssh/sshd_config

# echo "PasswordAuthentication no" >> /etc/ssh/sshd_config
# echo "KbdInteractiveAuthentication no" >> /etc/ssh/sshd_config



if [ $database_type == 'mysql' ]; then
    echo "########################### Start MySQL ######################################"
    /usr/libexec/mysqldtoto --user=root &
else
    echo "########################### Start MariaDB ######################################"
    if [ "$distrib" = "ALMALINUX" ]; then
      mariadbd --socket=/var/lib/mysql/mysql.sock --user=root > /dev/null 2>&1 &
    else
      mariadbd --socket=/run/mysqld/mysqld.sock --user=root > /dev/null 2>&1 &
    fi
    sleep 5

fi


echo "########################## Install centreon collect ###########################"
echo "Installation..."
if [ "$distrib" = "ALMALINUX" ]; then
  dnf clean all
  rm -f ./*-selinux-*.rpm # avoid to install selinux packages which are dependent to centreon-common-selinux
  dnf install -y ./*.rpm
else
  apt-get update
  apt-get install -y ./*.deb
fi


ulimit -c unlimited
ulimit -S -n 524288
echo '/tmp/core.%p' > /proc/sys/kernel/core_pattern

echo "########################## test connector ssh ###########################"

useradd -m -d /home/testconnssh testconnssh
echo testconnssh:passwd | chpasswd
su testconnssh -c "ssh-keygen -q -t rsa -N '' -f ~testconnssh/.ssh/id_rsa"
#ssh-keygen -q -t rsa -N '' -f ~/.ssh/id_rsa

cat ~testconnssh/.ssh/id_rsa.pub ~root/.ssh/id_rsa.pub > ~testconnssh/.ssh/authorized_keys
chown testconnssh: ~testconnssh/.ssh/*
chmod 600 ~testconnssh/.ssh/*

for ip_end in `echo "1 2 3 4 5 6 7 8 9 10 11 12"`; do
  ip="127.0.0.$ip_end"
  echo "Adding $ip to known_hosts"
  echo -n "$ip " >> ~/.ssh/known_hosts
  cat /etc/ssh/ssh_host_rsa_key.pub >> ~/.ssh/known_hosts
done

printf '2\0005\0003600\0001742811397\000/usr/lib64/nagios/plugins/check_by_ssh -H 127.0.0.2 -l testconnssh -a passwd -C "echo -n toto=turlututu"\0\0\0\0' > ssh_connector_cmd2.txt

/usr/lib64/centreon-connector/centreon_connector_ssh --debug --test-file ssh_connector_cmd2.txt &

sleep 30

echo "########################## logs sshd ###########################"
cat /tmp/sshd.log




#remove git dubious ownership
/usr/bin/git config --global --add safe.directory $PWD

echo "###### git clone opentelemetry-proto  #######"
git clone --depth=1 --single-branch https://github.com/open-telemetry/opentelemetry-proto.git opentelemetry-proto

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot -e unstable -t Test6Hosts $test_file

cat /etc/security/access.conf

ls -al /home/testconnssh
ls -al /home/testconnssh/.ssh

ssh -i ~/.ssh/id_rsa testconnssh@127.0.0.1 ls -al
