# Changelog

## 22.04.2

### Broker

#### Enhancements

*lua*
* stream connector accepts empty parameter values
* stream connector can read repeated fields in protobuf events.

*sql*

* An SQL error is no more fatal for cbd. The stream should not restart when errors
are raised.
* If Broker is connected to A MariaDB server 10.4 or newer, broker uses prepared statements in bulk to send data.

#### Fixes

*bam*

last\_level in KPIs is a float number and must be read as such.

*core*

* Broker is no more blocked when rows with unexpected values are encountered.
  An error log is raised to help the user to fix it, that's all.
* File failovers have been no more supported for an almost long time now. In
  this new version, this is not a blocking point. Some error logs are raised to
  help the user to fix his configuration but Broker does not stop.
* A possible deadlock has been removed from stats center.

*rrd*

* RRD graphs rebuilds are complete now.
* RRD graphs rebuilds flush data when the rebuild is over. This seems to improve
  the user experience.

### Engine

#### Enhancements

Anomaly detection can do the check if it's in a no ok state
#### Fixes

Engine waits up to 5 seconds to send bye event to broker

### perl connector

#### Fixes

* correct status is forwarded to engine
* When Engine was restarted, unacknowledged services/hosts could have their
  acknowledgements to come back. This is fixed.

## 22.04.1

### ccc

First version of ccc. Here is a client that can connect to broker or engine
through the gRPC server. Its goal is then to execute available methods on
these interfaces. At the moment, it checks the connection, tells if it was
established on engine or broker and is also able to list available methods.

### Broker

#### Enhancements

*grpc*

The gRPC api only listens by default on localhost. This is customizable with
the configuration file.

#### Fixes

*rrd*

Rebuilding/removing graphs is reenabled through database and a broker reload.

*main*

-s option works and can return errors if bad value entered

*GRPC stream*

* Doesn't coredump if connection fails on start process.
* The gRPC stream connector did not stop correctly on cbd stop.

*BAM*

On BAM misconfiguration, cbd could crash. This is fixed now. That was due to
an issue in mysql code with promises handling.

In a BA configured to ignore its kpi downtimes, if a kpi represented by a
service has two overlapping downtimes applied. When the first one is cancelled,
it is as if all the downtimes are removed. This is fixed with this new version.

*Debian*

Default configuration files were not installed on a Debian fresh install.

*unified_sql*

tags are well removed now.

Columns notes, notes\_url and action\_url are resized.

*Compression*
In the bbdo negotiation, compression was never activated

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

*grpc*

The gRPC api only listens by default on localhost. This is customizable with
the configuration file.

*comments*

They are sent only once to broker.

*multiline passive checks*

Escaped characters were not unescaped and we could not get them back.
