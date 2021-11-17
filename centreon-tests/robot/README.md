# centreon-tests

This folder contains tests for centreon broker and engine


## Getting Started

to run the tests you need to clone this repository into centreon vm or docker image, then you need to install robot framework

`pip3 install robotframework`
`pip3 install -U robotframework-databaselibrary`
`pip install pymysql`

and to run tests, you can use the following commands

`robot .`
or for selected test 
`robot broker/sql.robot`

## Broker 

### broker

- [x] BSS1: Start-Stop two instances of broker and no coredump
- [x] BSS2: Start/Stop 10 times broker with 300ms interval and no coredump
- [x] BSS3: Start-Stop one instance of broker and no coredump
- [x] BSS4: Start/Stop 10 times broker with 1sec interval and no coredump
- [x] BSS5: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
- [x] BDB1: Access denied when database name exists but is not the good one for sql output
- [x] BDB2: Access denied when database name exists but is not the good one for storage output
- [x] BDB3: Access denied when database name does not exist for sql output
- [x] BDB4: Access denied when database name does not exist for storage output
- [x] BDB5: cbd does not crash if the storage db_host is wrong
- [x] BDB6: cbd does not crash if the sql db_host is wrong
- [x] BDB7: access denied when database user password is wrong for perfdata/sql
- [x] BDB8: access denied when database user password is wrong for perfdata
- [x] BDB9: access denied when database user password is wrong for sql
- [x] BDB10: connection should be established when user password is good for sql/perfdata
- [x] BEDB2: start broker/engine and then start MariaDB => connection is established
- [x] BDBM1: start broker/engine and then start MariaDB => connection is established

## broker/engine

- [x] BECT1: Broker/Engine communication with anonymous TLS between central and poller
- [x] BECT2: Broker/Engine communication with TLS between central and poller with key/cert
- [x] BECT3: Broker/Engine communication with anonymous TLS and ca certificate
- [x] BECT4: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
- [x] BESS1: Start-Stop Broker/Engine - Broker started first - Broker stopped first
- [x] BESS2: Start-Stop Broker/Engine - Broker started first - Engine stopped first
- [x] BESS3: Start-Stop Broker/Engine - Engine started first - Engine stopped first
- [x] BESS4: Start-Stop Broker/Engine - Engine started first - Broker stopped first
- [x] BESS5: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
- [x] BRRDDM1: RRD metric deletion
- [x] Broker reverse connection good
- [x] Broker reverse connection too slow
- [x] Broker reverse connection stopped
- [x] New host group
- [x] BECC1: Broker/Engine communication with compression between central and poller
- [x] New host group
- [x] New host group

## Centengine

- [x] ESS1: Start-Stop one instance of engine and no coredump
- [x] ESS2: Start-Stop many instances of engine and no coredump
- [x] EPC1: Check with perl connector

