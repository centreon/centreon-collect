# Changelog

## 22.04.1

### Broker

#### Fixes

*main*

-s option works and can return errors if bad value entered

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

### Clib

#### Fixes

*Debian*

Packaging did not follow Debian good practices.

### Engine

#### Enhancements

*comments*

They are sent only once to broker.

*multiline passive checks*

Escaped characters were not unescaped and we could not get them back.
