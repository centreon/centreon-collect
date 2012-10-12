############
Installation
############

Merethis recommend using its official packages from the Centreon
Entreprise Server (CES) repository. Most of Merethis' endorsed
software are available as RPM packages.

Alternatively, you can build and install your own version of this
software by following the :ref:`user_installation_using_sources`.

**************
Using packages
**************

Merethis provides RPM for its products through Centreon Entreprise
Server (CES). Open source products are freely available from our
repository.

These packages have been successfully tested with CentOS 5 and RedHat 5.

Prerequisites
=============

In order to use RPM from the CES repository, you have to install the
appropriate repo file. Run the following command as privileged user::

  $ wget http://yum.centreon.com/standard/ces-standard.repo -O /etc/yum.repos.d/ces-standard.repo

The repo file is now installed.

Install
=======

Run the following commands as privileged user::

  $ yum clean all
  $ yum install centreon-clib centreon-clib-devel

All dependencies are automatically installed from Merethis repositories.

.. _user_installation_using_sources:

*************
Using sources
*************

To build Centreon Clib, you will need the following external
dependencies:

* a C++ compilation environment.
* CMake **(>=2.8)**, a cross-platform build system.

This program is compatible only with Unix-like platforms (Linux,
FreeBSD, Solaris, ...).

Prerequisites
=============

CentOS 5.x
----------

In CentOS 5.x you need to add manually cmake. After that you can
install binary packages. Either use the Package Manager or the
yum tool to install them. You should check packages version when
necessary.

Package required to build:

=========================== ================= ================================
Software                    Package Name      Description
=========================== ================= ================================
C++ compilation environment gcc gcc-c++ make  Mandatory tools to compile.
CMake **(>= 2.8)**          cmake             Read the build script and
                                              prepare sources for compilation.
=========================== ================= ================================

#. Get and install cmake form official website::

    $ wget http://www.cmake.org/files/v2.8/cmake-2.8.6-Linux-i386.sh
    $ sh cmake-2.8.6-Linux-i386.sh
    $ y
    $ y
    $ mv cmake-2.8.6-Linux-i386 /usr/local/cmake

#. Add cmake directory into the PATH environment variable::

    $ export PATH="$PATH:/usr/local/cmake/bin"

#. Install basic compilation tools::

    $ yum install gcc gcc-c++ make

CentOS 6.x
----------

FIXME

Debian/Ubuntu
-------------

In recent Debian/Ubuntu versions, necessary software is available as
binary packages from distribution repositories. Either use the Package
Manager or the apt-get tool to install them. You should check packages
version when necessary.

Package required to build:

=========================== ================ ================================
Software                    Package Name     Description
=========================== ================ ================================
C++ compilation environment build-essential  Mandatory tools to compile.
CMake **(>= 2.8)**          cmake            Read the build script and
                                             prepare sources for compilation.
=========================== ================ ================================

#. Install compilation tools::

    $ apt-get install build-essential cmake

OpenSUSE
--------

In recent OpenSUSE versions, necessary software is available as binary
packages from OpenSUSE repositories. Either use the Package Manager or
the zypper tool to install them. You should check packages version
when necessary.

Package required to build:

=========================== ================= ================================
Software                    Package Name      Description
=========================== ================= ================================
C++ compilation environment gcc gcc-c++ make  Mandatory tools to compile.
CMake **(>= 2.8)**          cmake             Read the build script and
                                              prepare sources for compilation.
=========================== ================= ================================

#. Install compilation tools::

    $ zypper install gcc gcc-c++ make cmake

Build
=====

Get sources
-----------

Centreon Clib can be checked out from Merethis's git server at
http://git.centreon.com/centreon-clib. On a Linux box with git
installed this is just a matter of::

  $ git clone http://git.centreon.com/centreon-clib

Configuration
-------------

At the root of the project directory you'll find a build directory
which holds build scripts. Generate the Makefile by running the
following command::

  $ cd /path_to_centreon_clib/build
  $ cmake .

Checking of necessary components is performed and if successfully
executed a summary of your configuration is printed.

Variables
~~~~~~~~~

Your Centreon Clib can be tweaked to your particular needs using CMake's
variable system. Variables can be set like this::

  $ cmake -D<variable1>=<value1> [-D<variable2>=<value2>] .

Here's the list of variables available and their description:

============================== =============================================== ======================================
Variable                        Description                                    Default value
============================== =============================================== ======================================
WITH_PKGCONFIG_DIR              Use to install pkg-config files.               ${WITH_PREFIX_LIB}/pkgconfig
WITH_PKGCONFIG_SCRIPT           Enable or disable install pkg-config files.    ON
WITH_PREFIX                     Base directory for Centreon Clib installation. /usr/local
                                If other prefixes are expressed as relative
                                paths, they are relative to this path.
WITH_PREFIX_INC                 Define specific directory for Centreon Engine  ${WITH_PREFIX}/include/centreon-engine
                                headers.
WITH_PREFIX_LIB                 Define specific directory for Centreon Engine  ${WITH_PREFIX}/lib/centreon-engine
                                modules.
WITH_SHARED_LIB                 Create or not a shared library.                ON
WITH_STATIC_LIB                 Create or not a static library.                OFF
WITH_TESTING                    Build unit test.                               OFF
============================== =============================================== ======================================

Example::

  $ cmake \
     -DWITH_TESTING=0 \
     -DWITH_PREFIX=/usr \
     -DWITH_PREFIX_LIB=/usr/lib \
     -DWITH_PREFIX_INC=/usr/include/centreon-clib \
     -DWITH_SHARED_LIB=1 \
     -DWITH_STATIC_LIB=0 \
     -DWITH_PKGCONFIG_DIR=/usr/lib/pkgconfig .

At this step, the software will check for existence and usability of the
rerequisites. If one cannot be found, an appropriate error message will
be printed. Otherwise an installation summary will be printed.

Compilation
-----------

Once properly configured, the compilation process is really simple::

  $ make

And wait until compilation completes.

Install
=======

Once compiled, the following command must be run as privileged user to
finish installation::

  $ make install

And wait for its completion.
