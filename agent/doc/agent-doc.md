# Centreon Monitoring Agent documentation {#mainpage}

## Introduction

The purpose of this program is to run checks on the Windows and Linux operating systems. It is entirely asynchronous, with the exception of the gRPC layers. It is also single-threaded and therefore needs no mutexes, except in the gRPC part.
This is why, when a request is received, it is posted to ASIO for processing in the main thread.

## Configuration
The configuration is given by Engine by an AgentConfiguration message sent over gRPC.
The configuration object is embedded in MessageToAgent::config

## Scheduler
Scheduler is created when a server service (revers connection is created) or when agent creates a service client.
At this time he doesn't know the list of checks to execute. He begins to execute checks when he receives the an AgentConfiguration message.
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


## native checks
All checks are scheduled by one thread, no mutex needed.
In order to add a native check, you need to inherit from check class. 
Then you have to override constructor and start_check method.
All is asynchronous. When start_check is called, it must not block caller for a long time. 
At the end of measure, it must call check::on_completion().
That method need 4 arguments:
* start_check_index: For long asynchronous operation, at the beginning, asynchronous job must store running_index and use it when he have to call check::on_completion(). It is useful for scheduler to check is the result is the result of the last asynchronous job start. The new class can get running index with check::_get_running_check_index()
* status: plugins status equivalent. Values are 0:Ok, 1: warning, 2: critical, 3: unknown (https://nagios-plugins.org/doc/guidelines.html#AEN41)
* perfdata: a list of com::centreon::common::perfdata objects
* outputs: equivalent of plugins output as "CPU 54% OK"

BEWARE, in some cases, we can have recursion, check::on_completion can call start_check

A little example:
```c++
class dummy_check : public check {
  duration _command_duration;
  asio::system_timer _command_timer;

 public:
  void start_check(const duration& timeout) override {
    check::start_check(timeout);
    _command_timer.expires_from_now(_command_duration);
    _command_timer.async_wait([me = shared_from_this(), this,
                               running_index = _get_running_check_index()](
                                  const boost::system::error_code& err) {
      if (err) {
        return;
      }
      on_completion(running_index, 1,
                    std::list<com::centreon::common::perfdata>(),
                    {"output dummy_check of " + get_command_line()});
    });
  }

  template <typename handler_type>
  dummy_check(const std::string& serv,
              const std::string& command_name,
              const std::string& command_line,
              const duration& command_duration,
              handler_type&& handler)
      : check(g_io_context,
              spdlog::default_logger(),
              std::chrono::system_clock::now(),
              serv,
              command_name,
              command_line,
              nullptr,
              handler),
        _command_duration(command_duration),
        _command_timer(*g_io_context) {}
};
```