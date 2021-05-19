# centreon-tests

This repository contain tests for centreon broker and engine


## Getting Started

to run the tests you need to clone this repository into centreon vm or docker image, then you need to install npm dependencies

`npm install`

and to run tests, you can use the following commands

`npm run test`
`npm run test -- src/ceentreon/start-stop.test.ts`


## Broker 

### Start and stop test

- [x] start/stop centreon broker
- [x] start and stop many instances broker with .3 seconds interval
- [x] start and stop many instances broker with 1 second interval

## Bad credentials

- [x] should deny access when database user password is wrong
- [x] should deny access when database user password is wrong for storage
- [x] should deny access when database user password is wrong for sql
- [ ] should log error when mariadb server is stopped

