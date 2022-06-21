# Changelog

## 22.04.1

### ccc

First version of ccc. Here is a client that can connect to broker or engine
through the gRPC server. Its goal is then to execute available methods on
these interfaces. At the moment, it checks the connection, tells if it was
established on engine or broker and is also able to list available methods.

### Broker

#### Fixes

*main*

-s option works and can return errors if bad value entered

*stream gRPC*

A gRPC stream connector did not stop correctly on cbd stop.

*BAM*

On BAM misconfiguration, cbd could crash. This is fixed now. That was due to
an issue in mysql code with promises handling.

*Debian*

Default configuration files were not installed on a Debian fresh install.

*unified_sql*

tags are well removed now.

Columns notes, notes\_url and action\_url are resized.

*Debian*

Default configuration files were not installed on a Debian fresh install.

*GRPC stream*

Don't coredump if connection fail on process start

#### Enhancements

*downtimes*

They are inserted in bulk now.

*sql*

The mysql socket is defined with:
* /var/run/mysqld/mysqld.sock on Debian and similar distribs
* /var/lib/mysql/mysql.sock on RH and similar distribs
* /tmp/mysql.sock on others

### Clib

#### Fixes

*Debian*

Packaging did not follow Debian good practices.

### Engine

#### Bugfixes

*resources*

The display\_name of resources could be emptied in several case of reload.

#### Enhancements

*comments*

They are sent only once to broker.

*multiline passive checks*

Escaped characters were not unescaped and we could not get them back.
