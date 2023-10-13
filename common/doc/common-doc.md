# Common library documentation {#mainpage}

## log\_v2

This library is used by both centreon-engine and centreon-broker. It is built
on top of [the Spdlog library](https://github.com/gabime/spdlog/wiki/1.-QuickStart).

At the beginning of the software, logs have to be initialized. We have to provide
a name and a list of logger names. It is not mandatory to declare all the possible
logger names, it is always possible to declare them later.

For centreon-engine and centreon-broker, we always declare at least two loggers
that are *core* and *config*. The order they are declared is the same as the
order we access to each one. So the core logger is always at index 0 and the
config logger is always at index 1.
