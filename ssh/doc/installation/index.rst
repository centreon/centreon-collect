############
Installation
############

Centreon recommends using its official packages from the Centreon
Entreprise Server (CES) repository. Most of Centreon' endorsed
software are available as RPM packages.

Alternatively, you can build and install your own version of this
software by following the :ref:`user_installation_using_sources`.

**************
Using packages
**************

Centreon provides RPM for its products through Centreon Entreprise
Server (CES). Open source products are freely available from our
repository.

These packages have been successfully tested with CentOS 5 and RedHat 5.

.. _user_installation_packages_prerequisites:

Prerequisites
=============

In order to use RPM from the CES repository, you have to install the
appropriate repo file. Run the following command as privileged user ::

  $ wget http://yum.centreon.com/standard/2.2/ces-standard.repo -O /etc/yum.repos.d/ces-standard.repo

The repo file is now installed. Don't forget to cleanup ::

  $ yum clean all

Install
=======

Run the following commands as privileged user ::

  $ yum install centreon-connector-ssh

All dependencies are automatically installed from Centreon repositories.

.. _user_installation_using_sources:

*************
Using sources
*************

To build Centreon SSH Connector, you will need the following external
dependencies:

  * a C++ compilation environment.
  * CMake **(>= 2.8)**, a cross-platform build system.
  * Centreon Clib, The centreon Core library.
  * ssh2 library to use ssh functions.
  * gcrypt library to secure connections.

This program is compatible only with Unix-like platforms (Linux,
FreeBSD, Solaris, ...).

Prerequisites
=============

CentOS
------

In CentOS you need to add manually cmake. After that you can
install binary packages. Either use the Package Manager or the
yum tool to install them. You should check packages version when
necessary.

Package required to build:

=========================== =================== ================================
Software                     Package Name       Description
=========================== =================== ================================
C++ compilation environment gcc gcc-c++ make    Mandatory tools to compile.
CMake **(>= 2.8)**          cmake               Read the build script and
                                                prepare sources for compilation.
Centreon Clib (>= 1.0)      centreon-clib-devel Core library used by Centreon
                                                Connector.
ssh2 library                libssh2-devel       SSH library.
gcrypt library              libgcrypt-devel     Gcrypt library.
=========================== =================== ================================

#. Install basic compilation tools ::

   $ yum install gcc gcc-c++ make libssh2-devel libgcrypt-devel

#. Install Centreon repository

   You need to install Centreon Entreprise Server (CES) repos file as
   explained :ref:`user_installation_packages_prerequisites` to use some
   specific package version.

#. Install cmake ::

   $ yum install cmake

#. Install Centreon Clib

   See the Centreon Clib :ref:`documentation <centreon-clib:centreon_clib_install>`.

Debian/Ubuntu
-------------

In recent Debian/Ubuntu versions, necessary software is available as
binary packages from distribution repositories. Either use the Package
Manager or the apt-get tool to install them. You should check packages
version when necessary.

Package required to build:

=========================== ================= ================================
Software                    Package Name      Description
=========================== ================= ================================
C++ compilation environment build-essential   Mandatory tools to compile.
CMake **(>= 2.8)**          cmake             Read the build script and
                                              prepare sources for compilation.
Centreon Clib               centreon-clib-dev Core library used by Centreon
                                              Connector.
ssh2 library                libssh2-1-dev     SSH library.
gcrypt library              libgcrypt11-dev   Gcrypt library.
=========================== ================= ================================

#. Install compilation tools ::

   $ apt-get install build-essential cmake libssh2-1-dev libgcrypt11-dev

#. Install Centreon Clib

   See the Centreon Clib :ref:`documentation <centreon-clib:centreon_clib_install>`.

OpenSUSE
--------

In recent OpenSUSE versions, necessary software is available as binary
packages from OpenSUSE repositories. Either use the Package Manager or
the zypper tool to install them. You should check packages version
when necessary.

Package required to build:

=========================== =================== ================================
Software                    Package Name        Description
=========================== =================== ================================
C++ compilation environment gcc gcc-c++ make    Mandatory tools to compile.
CMake **(>= 2.8)**          cmake               Read the build script and
                                                prepare sources for compilation.
Centreon Clib               centreon-clib-devel Core library used by Centreon
                                                Connector.
ssh2 library                libssh2-devel       SSH library.
gcrypt library              libgcrypt-devel     Gcrypt library.
=========================== =================== ================================

#. Install compilation tools ::

   $ zypper install gcc gcc-c++ make cmake libssh2-devel libgcrypt-devel

#. Install Centreon Clib

   See the Centreon Clib :ref:`documentation <centreon-clib:centreon_clib_install>`.

Build
=====

Get sources
-----------

Centreon SSH Connector can be checked out from GitHub at
https://github.com/centreon/centreon-connectors . The SSH connector
sources reside in the ssh subdirectory. On a Linux box with git
installed this is just a matter of ::

  $ git clone https://github.com/centreon/centreon-connectors

Or You can get the latest Centreon Connector's sources from its
`download website <http://www.centreon.com/Content-Download/download-centreon-connectors>`_.
Once downloaded, extract it ::

  $ tar xzf centreon-connector.tar.gz

Configuration
-------------

At the root of the project directory you'll find a ssh/build directory
which holds build scripts. Generate the Makefile by running the
following command ::

  $ cd /path_to_centreon_connector/ssh/build

Your Centreon SSH Connector can be tweaked to your particular needs
using CMake's variable system. Variables can be set like this ::

  $ cmake -D<variable1>=<value1> [-D<variable2>=<value2>] .

Here's the list of variables available and their description:

============================== ================================================ ======================
Variable                       Description                                      Default value
============================== ================================================ ======================
WITH_CENTREON_CLIB_INCLUDE_DIR Set the directory path of centreon-clib include. auto detection
WITH_CENTREON_CLIB_LIBRARIES   Set the centreon-clib library to use.            auto detection
WITH_CENTREON_CLIB_LIBRARY_DIR Set the centreon-clib library directory (don't   auto detection
                               use it if you use WITH_CENTREON_CLIB_LIBRARIES)
WITH_KNOWN_HOSTS_CHECK         Enable or disable Check hosts against user's     OFF
                               known_hosts file.
WITH_LIBGCRYPT_INCLUDE_DIR     Set the directory path of libgcrypt include.     auto detection
WITH_LIBGCRYPT_LIBRARIES       Set the libgcrypt library to use.                auto detection
WITH_LIBGCRYPT_LIBRARY_DIR     Set the libgcrypt library directory (don't       auto detection
                               use it if you use WITH_LIBGCRYPT_LIBRARIES)
WITH_LIBSSH2_INCLUDE_DIR       Set the directory path of libssh2 include.       auto detection
WITH_LIBSSH2_LIBRARIES         Set the libssh2 library to use.                  auto detection
WITH_LIBSSH2_LIBRARY_DIR       Set the libssh2 library directory (don't use     auto detection
                               it if you use WITH_LIBSSH2_LIBRARIES)
WITH_PREFIX                    Base directory for Centreon SSH Connector        ``/usr/local``
                               installation. If other prefixes are expressed
                               as relative paths, they are relative to this
                               path.
WITH_PREFIX_BINARY             Define specific directory for Centreon           ``${WITH_PREFIX}/bin``
                               Connector SSH binary.
WITH_TESTING                   Enable generation of unit tests. They can        OFF
                               later be run by typing *make test*.
============================== ================================================ ======================

Example ::

  $ cmake \
     -DWITH_PREFIX=/usr \
     -DWITH_PREFIX_BINARY=/usr/lib/centreon-connector \
     -DWITH_TESTING=0 .

At this step, the software will check for existence and usability of the
rerequisites. If one cannot be found, an appropriate error message will
be printed. Otherwise an installation summary will be printed.

.. note::
  If you need to change the options you used to compile your software,
  you might want to remove the *CMakeCache.txt* file that is in the
  *build* directory. This will remove cache entries that might have been
  computed during the last configuration step.

Compilation
-----------

Once properly configured, the compilation process is really simple ::

  $ make

And wait until compilation completes.

Install
=======

Once compiled, the following command must be run as privileged user to
finish installation ::

  $ make install

And wait for its completion.
