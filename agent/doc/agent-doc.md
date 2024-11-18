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

This means that the second check may start later than the scheduled time point (12:00:10) if the first checks take too long.

When a check completes, it is inserted into _waiting_check_queue, and its start will be scheduled as soon as a slot in the queue is available (the queue is a set indexed by expected_start) minus old_start plus check_period.


## native checks
All checks are scheduled by one thread, no mutex needed.
In order to add a native check, you need to inherit from check class. 
Then you have to override constructor and start_check method.
All is asynchronous. When start_check is called, it must not block caller for a long time. 
At the end of measure, it must call check::on_completion().
That method need 4 arguments:
* start_check_index: For long asynchronous operation, at the beginning, asynchronous job must store running_index and use it when he has to call check::on_completion(). It is useful for scheduler to check if it's the result of the last asynchronous job start. The new class can get running index with check::_get_running_check_index()
    An example, checks starts a first measure, the timeout expires, a second measure starts, the first measure ends,we don't take into account his result and we wait for the end off second one.
* status: plugins status equivalent. Values are 0:Ok, 1: warning, 2: critical, 3: unknown (https://nagios-plugins.org/doc/guidelines.html#AEN41)
* perfdata: a list of com::centreon::common::perfdata objects
* outputs: equivalent of plugins output as "CPU 54% OK"

A little example:
```c++
class dummy_check : public check {
  duration _command_duration;
  asio::system_timer _command_timer;

 public:
  void start_check(const duration& timeout) override {
    if (!check::start_check(timeout)) {
      return;
    }
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
              std::chrono::seconds(1),
              serv,
              command_name,
              command_line,
              nullptr,
              handler),
        _command_duration(command_duration),
        _command_timer(*g_io_context) {}
};
```

### native_check_cpu (linux version)
It uses /proc/stat to measure cpu statistics. When start_check is called, a first snapshot of /proc/stat is done. Then a timer is started and will expires at max time_out or check_interval minus 1 second. When this timer expires, we do a second snapshot and create plugin output and perfdata from this difference.
The arguments accepted by this check (in json format) are:
* cpu-detailed: 
  * if false, produces only average cpu usage perfdata per processor and one for the average 
  * if true, produces per processor and average one perfdata for user, nice, system, idle, iowait, irq, soft_irq, steal, guest, guest_nice and total used counters

Output is inspired from centreon local cpu and cpu-detailed plugins
Examples of output: 
* OK: CPU(s) average usage is 24.08%
* CRITICAL: CPU'0' Usage: 24.66%, User 17.58%, Nice 0.00%, System 5.77%, Idle 75.34%, IOWait 0.39%, Interrupt 0.00%, Soft Irq 0.91%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU'2' Usage: 24.18%, User 17.69%, Nice 0.00%, System 5.99%, Idle 75.82%, IOWait 0.38%, Interrupt 0.00%, Soft Irq 0.12%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% CRITICAL: CPU(s) average Usage: 24.08%, User 17.65%, Nice 0.00%, System 5.80%, Idle 75.92%, IOWait 0.36%, Interrupt 0.00%, Soft Irq 0.27%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00%
  
Example of perfdatas in not cpu-detailed mode: 
* cpu.utilization.percentage
* 0#core.cpu.utilization.percentage
* 1#core.cpu.utilization.percentage

Example of perfdatas in cpu-detailed mode:
* 0~user#core.cpu.utilization.percentage
* 0~system#core.cpu.utilization.percentage
* 1~interrupt#core.cpu.utilization.percentage
* iowait#cpu.utilization.percentage
* used#cpu.utilization.percentage

### native_check_cpu (windows version)
metrics aren't the same as linux version. We collect user, idle, kernel , interrupt and dpc times.

There are two methods, you can use internal microsoft function NtQuerySystemInformation. Yes Microsoft says that they can change signature or data format at any moment, but it's quite stable for many years. A trick, idle time is included un kernel time, so we subtract first from the second. Dpc time is yet included in interrupt time, so we don't sum it to calculate total time.
The second one relies on performance data counters (pdh API), it gives us percentage despite that sum of percentage is not quite 100%. That's why the default method is the first one.
The choice between the two methods is done by 'use-nt-query-system-information' boolean parameter.

### check_drive_size
we have to get free space on server drives. In case of network drives, this call can block in case of network failure. Unfortunately, there is no asynchronous API to do that. So a dedicated thread (drive_size_thread) computes these statistics. In order to be os independent and to test it, drive_size_thread relies on a functor that do the job: drive_size_thread::os_fs_stats. This functor is initialized in main function. drive_size thread is stopped at the end of main function.

So it works like that:
* check_drive_size post query in drive_size_thread queue
* drive_size_thread call os_fs_stats
* drive_size_thread post result in io_context
* io_context calls check_drive_size::_completion_handler