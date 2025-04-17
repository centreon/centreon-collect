# Core

## Description

The first process of gorgone is responsible for more than launching other module, and babysitting them.

It also handle part of the log synchronization from distant poller, transforming log to response from zmq message, 
and run the check() sub of each module regulary.

## house keeping
Every 5 second gorgone use EV to run a series of house keeping task, orchestrated by periodic_exec().
It will run a "check()" sub defined by the module, generally hooks.pm.

The sub is run by the main process and not the child process, but is defined in each module. See each module for a description of the behaviour of the check().
