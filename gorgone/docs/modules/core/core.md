# Core

## Description

Gorgone's first process is responsible for more than launching other modules, and babysitting them.

It also handles part of the log synchronization from distant pollers, transforming logs to responses from zmq messages, 
and runs the check() sub of each module regulary.

## Housekeeping

Every 5 seconds, Gorgone uses EV to run a series of housekeeping tasks, orchestrated by periodic_exec().
It will run a "check()" sub defined by the module, generally hooks.pm.

The sub is run by the main process and not the child process, but is defined in each module. See each module for a description of the behaviour of the check().
