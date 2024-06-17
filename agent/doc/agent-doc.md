# Centreon Agent documentation {#mainpage}

## Introduction

The goal of this program is to execute checks in both windows and linux OS
It's full asynchronous, excepted grpc layers, it's single threaded and you won't find mutex in code.

## Configuration
configuration is given by Engine by a AgentConfiguration sent over grpc
The configuration object is embedded in EngineToAgent::config

## Scheduler
We trie to spread checks over check_period.
Example: We have 10 checks to execute during one second. check1 will start at now + 0.1s, second at now + 0.2s..

When Agent receives configuration, all checks are recreated.
For example, we have 100 checks to execute in 10 minute, at it is 12:00:00.
First  service check will start right now, second one at 12:00:06, third at 12:00:12... and the last one at 12:09:54
We don't care about tests duration, we work with time points. 
In the previous example, time of second check of first service will be scheduled at 12:00:10 even if all other checks has not been yet started.

In case of check duration is too long, we might exceed maximum of concurrent checks. In that case checks will b executed as soon one will be ended.
So second check may start later than scheduled time point (12:00:10) if other first checks are too long. Order of checks is always respected even in case of bottleneck.
