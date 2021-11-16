# centreon-tests

This folder contain tests for centreon broker and engine


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

### Start and stop test

- [x] start/stop centreon broker
- [x] start and stop multiple times

## Bad credentials

- [x] should deny access when database user password is wrong
- [x] should deny access when database user password is wrong for storage
- [x] should deny access when database user password is wrong for sql
- [x] should log error when database name is not correct
- [ ] should log error when mariadb server is stopped

## Centengine

- [x] start/stop centreon centgine
- [x] start/stop centreon centengine multiple times

