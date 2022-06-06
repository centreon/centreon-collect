# Changelog

## 22.04.1

### Broker

#### Fixes

*stream gRPC*

A gRPC stream connector did not stop correctly on cbd stop.

*BAM*

On BAM misconfiguration, cbd could crash. This is fixed now. That was due to
an issue in mysql code with promises handling.

*Debian*

Default configuration files were not installed on a Debian fresh install.

*unified_sql*

tags are well removed now.

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
