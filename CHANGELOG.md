# Changelog

## 22.10.1

### Fixes

#### Broker

*Lua*
* Lua accepts repeated fields from Protobuf events.

#### Engine

* When Engine was restarted, services/hosts acknowledgements could come back.

### Enhancements

#### Broker

*Sql*

In case of a connection to MariaDB server 10.4 or newer, broker now sends data with prepared statements in bulk.

#### Engine

