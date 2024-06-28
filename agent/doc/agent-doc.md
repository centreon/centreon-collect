# Centreon Monitoring Agent documentation {#mainpage}

## Introduction

The purpose of this program is to run checks on the Windows and Linux operating systems. It is entirely asynchronous, with the exception of the gRPC layers. It is also single-threaded and therefore needs no mutexes, except in the gRPC part.
This is why, when a request is received, it is posted to ASIO for processing in the main thread.

## Configuration
The configuration is given by Engine by an AgentConfiguration message sent over gRPC.
The configuration object is embedded in MessageToAgent::config

## Scheduler
We try to spread checks over the check_period.
Example: We have 10 checks to execute during one second. Check1 will start at now, second at now + 0.1s..

When the Agent receives the configuration, all checks are recreated.
For example, we have 100 checks to execute in 10 minutes, at it is 12:00:00.
The first service check will start right now, the second one at 12:00:06, third at 12:00:12... and the last one at 12:09:54
We don't care about the duration of tests, we work with time points. 
In the previous example, the second check for the first service will be scheduled at 12:00:10 even if all other checks has not been yet started.

In case of check duration is too long, we might exceed maximum of concurrent checks. In that case checks will be executed as soon one will be ended.
This means that the second check may start later than the scheduled time point (12:00:10) if the other first checks are too long. The order of checks is always respected even in case of a bottleneck.
For example, a check lambda has a start_expected to 12:00, because of bottleneck, it starts at 12:15. Next start_expected of check lambda will then be 12:15 + check_period.
