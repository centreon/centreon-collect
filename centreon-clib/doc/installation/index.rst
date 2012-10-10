############
Installation
############

Merethis recommend using its official packages from the *Centreon
Entreprise Server* (aka *CES*) repository. Most of Merethis' endorsed
software are available as RPM packages.

Alternatively, you can build and install your own Centreon Clib software
by following the :ref:`using_sources`.

**************
Using packages
**************

Merethis provides RPM for its products through Centreon Entreprise
Server (CES). Open source products are freely available from our
repository.

These packages have been successfully tested with CentOS 5
and RedHat 5.

Prerequisites
=============

In order to use RPM from the CES repository, you have to install the
appropriate repo file. Run the following command as privileged user
(aka *root*)::

  $ wget http://yum.centreon.com/standard/ces-standard.repo -O /etc/yum.repos.d/ces-standard.repo

The repo file is now installed.

Install
=======

Run the following commands as privileged user (aka *root*)::

  $ yum clean all
  $ yum install centreon-clib

Centreon Clib and its dependencies are automatically installed from Merethis repositories.

.. _using_sources:

*************
Using sources
*************

To build Centreon Clib, you will need the following external
dependencies:

* a C++ compilation environment
* **CMake (>=2.8)**, a cross-platform build system

Centreon Clib is compatible only with Unix-like platforms (Linux,
FreeBSD, Solaris, …).

.. _prerequisites:

Prerequisites
=============

CentOS 5.x
----------

In CentOS 5.x you need to add manually cmake. After that you can
install binary packages. Either use the *Package Manager* or the
*yum* tool to install them. You should check packages version when
necessary.

Necessary package to build Centreon Clib :

=========================== ================= ==========================================================
Software                     Package Name     Description
=========================== ================= ==========================================================
C++ compilation environment  gcc gcc-c++ make Mandatory tools to compile Centreon Clib.
CMake (**>= 2.8**)           cmake            Read the build script and prepare sources for compilation.
=========================== ================= ==========================================================

#. Get and install cmake form official website::

    $ wget http://www.cmake.org/files/v2.8/cmake-2.8.6-Linux-i386.sh
    $ sh cmake-2.8.6-Linux-i386.sh
    $ y
    $ y
    $ mv cmake-2.8.6-Linux-i386 /usr/local/cmake

#. Add cmake directory into the PATH environment variable::

    $ export PATH="$PATH:/usr/local/cmake/bin"

#. Install basic compilation tools to build Centreon Clib::

    $ yum install gcc gcc-c++ make

CentOS 6.x
----------

FIXME

Debian/Ubuntu
-------------

In recent Debian versions, necessary software is available as binary
packages from distribution repositories. Either use the *Package Manager*
or the *apt-get* tool to install them. You should check packages
version when necessary.

Necessary package to build Centreon Clib:

=========================== ================ ==========================================================
Software                     Package Name    Description
=========================== ================ ==========================================================
C++ compilation environment  build-essential Mandatory tools to compile Centreon Clib.
CMake (>= 2.8)               cmake           Read the build script and prepare sources for compilation.
=========================== ================ ==========================================================

#. Install compilation tools::

    $ apt-get install build-essential cmake

OpenSUSE
--------

In recent OpenSUSE versions, necessary software is available as binary
packages from Ubuntu repositories. Either use the *Package Manager* or
the *zypper* tool to install them. You should check packages version
when necessary.

Necessary package to build Centreon Clib:

=========================== ================= ==========================================================
Software                     Package Name     Description
=========================== ================= ==========================================================
C++ compilation environment  gcc gcc-c++ make Mandatory tools to compile Centreon Clib.
CMake (>= 2.8)               cmake            Read the build script and prepare sources for compilation.
=========================== ================= ==========================================================

#. Install compilation tools::

    $ zypper install gcc gcc-c++ make cmake

Build
=====

Get sources
-----------

You can get last release of Centreon Clib sources
`here <http://forge.centreon.com/projects/centreon-clib/repository>`_.

Configuration
-------------

At the root of the project directory you'll find a **build** directory
which holds build scripts. Generate the Makefile by running the
following command (WITH_USER and WITH_GROUP as per
:ref:`prerequisites`::

  $ cmake .

CMake will check for all necessary dependencies and indicates if they
could not be found.

Variables
~~~~~~~~~

Your Centreon Clib can be tweaked to your particular needs using CMake's
variable system. Variables can be set like this::

  $ cmake -D<variable1>=<value1> [-D<variable2>=<value2>] .

Here's the list of variables available and their description:

============================== =================================================================================================================================== ========================================
Variable                        Description                                                                                                                          Default value
============================== =================================================================================================================================== ========================================
WITH_PKGCONFIG_DIR              Use to install pkg-config files.                                                                                                     ${WITH_PREFIX_LIB}/pkgconfig
WITH_PKGCONFIG_SCRIPT           Enable or disable install pkg-config files.                                                                                          ON
WITH_PREFIX                     Base directory for Centreon Clib installation. If other prefixes are expressed as relative paths, they are relative to this path.    /usr/local
WITH_PREFIX_INC                 Define specific directory for Centreon Engine headers.                                                                               ${WITH_PREFIX}/include/centreon-engine
WITH_PREFIX_LIB                 Define specific directory for Centreon Engine modules.                                                                               ${WITH_PREFIX}/lib/centreon-engine
WITH_SHARED_LIB                 Create or not a shared library.                                                                                                      ON
WITH_STATIC_LIB                 Create or not a static library.                                                                                                      OFF
WITH_TESTING                    Build unit test.                                                                                                                     OFF
============================== =================================================================================================================================== ========================================

Example::

  $ cmake \
     -DWITH_TESTING=0 \
     -DWITH_PREFIX=/usr \
     -DWITH_PREFIX_LIB=/usr/lib \
     -DWITH_PREFIX_INC=/usr/include/centreon-clib \
     -DWITH_SHARED_LIB=1 \
     -DWITH_STATIC_LIB=0 \
     -DWITH_PKGCONFIG_DIR=/usr/lib/pkgconfig .

Compilation
-----------

Once properly configured, the compilation process is really simple.
Just run::

  $ make

And wait until compilation completes.

Install
=======

Centreon Clib's installation process is pretty simple. Just run as
privileged user the command::

  $ make install

And wait for its completion.
