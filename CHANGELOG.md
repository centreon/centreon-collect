# Changelog

## 22.04.1

### Fixes

#### Broker

*stream gRPC*

A gRPC stream connector did not stop correctly on cbd stop.

*BAM*

On BAM misconfiguration, cbd could crash. This is fixed now. That was due to
an issue in mysql code with promises handling.

### Enhancements

#### Broker

*downtimes*

They are inserted in bulk now.

#### Engine

*comments*

They are sent only once to broker.
