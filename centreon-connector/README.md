# Centreon Connectors #

Centreon Connectors are extremely fast open-source monitoring check
execution daemons designed to work with
[Centreon Engine](https://github.com/centreon/centreon-engine).
It is a low-level component of the [Centreon software suite](https://www.centreon.com).

Centreon Connectors are released under the Apache Software License version 2
and is endorsed by the [Centreon company](https://www.centreon.com).

There are currently two open-source connectors :

* **Centreon Connector Perl** : persistent Perl interpreter that
  executes Perl plugins very fast
* **Centreon Connector SSH** : maintain SSH connexions opened to reduce
  overhead of plugin execution over SSH

## Documentation ##

The full Centreon Connector Perl documentation is available online
[here](http://documentation.centreon.com/docs/centreon-perl-connector/en/).
The full Centreon Connector SSH documentation is also available online
[here](http://documentation.centreon.com/docs/centreon-ssh-connector/en/).
The two documentation sets are generated from ReST files located in the
*./perl/doc/* and *./ssh/doc/* directories (respectively) of Centreon
Connectors sources.

Documentation extensively covers all aspects of Centreon Connectors such
as installation, compilation, configuration, use and more. It is the
reference guide of the software. This *README* is only provided as a
quick introduction.

## Installing from binaries ##

**Warning**: Centreon Connectors are low-level components of the
Centreon software suite. If this is your first installation you would
probably want to [install it entirely](https://documentation.centreon.com/docs/centreon/en/2.6.x/installation/index.html).

Centreon ([the company behind the Centreon software suite](http://www.centreon.com))
provides binary packages for RedHat / CentOS. They are available either
as part of the [Centreon Entreprise Server distribution](https://www.centreon.com/en/products/centreon-enterprise-server/)
or as individual packages on [our RPM repository](https://documentation.centreon.com/docs/centreon/en/2.6.x/installation/from_packages.html).

Once the repository installed a simple command will be needed to install
a connector.

    $# yum install centreon-connector-perl
    $# yum install centreon-connector-ssh

## Fetching sources ##

The reference repository is hosted at [GitHub](https://github.com/centreon/centreon-connectors).
Beware that the repository hosts in-developement sources and that it
might not work at all.

Stable releases are available as gziped tarballs on [Centreon's download site](https://download.centreon.com).

## Compilation (quickstart) ##

**Warning**: Centreon Connectors are low-level components of the
Centreon software suite. If this is your first installation you would
probably want to [install it entirely](https://documentation.centreon.com/docs/centreon/en/2.6.x/installation/index.html).

This paragraph is only a quickstart guide for the compilation of
Centreon Connectors. For a more in-depth guide with build options you should
refer to the online documentation of
[Centreon Connector Perl](http://documentation.centreon.com/docs/centreon-perl-connector/en/)
or [Centreon Connector SSH](http://documentation.centreon.com/docs/centreon-ssh-connector/en/).

Centreon Connectors need Centreon Clib to be build. You should
[install it first](https://github.com/centreon/centreon-clib).

Once the sources of Centreon Connectors extracted you should build each
project independently by going to its *build/* directory and launching
the CMake command. This will look for required dependencies and print a
summary of the compilation parameters if everything went fine. The
example below is for Centreon Connector Perl but works all the same for
any other Connector.

    $> cd centreon-connector/perl/build
    $> cmake .
    ...

Now launch the compilation using the *make* command and then install the
software by running *make install* as priviledged user.

    $> make -j 4
    ...
    $# make install

You're done !

## Bug reports ##

The best way to report a bug is to open an issue in GitHub's
[issue tracker](https://github.com/centreon/centreon-connectors/issues/).

For a quick resolution your message should contain :

* the problem description
* precise steps on how to reproduce the issue (if you're using Centreon
  web UI tell us where you click)
* the expected behavior
* the Centreon product**s** version**s**
* the operating system you're using (name and version)
* if possible configuration, log and debug files

## Contributing ##

Contributions are much welcome ! If possible provide them as
pull-requests on GitHub. If not, patches will do but describe against
which vesion/commit they apply.

For any question or remark feel free to send a mail to the project
maintainer : Matthieu Kermagoret (mkermagoret@centreon.com).

## Feature requests ##

Features can be requested in the
[issue tracker](https://github.com/centreon/centreon-connectors/issues/).
