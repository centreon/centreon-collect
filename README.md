Centreon Collect

Blablabla

Documentation

Need to talk about centralized doc

Installing from binaries
Warning: Centreon Broker is a low-level component of the Centreon software suite. If this is your first installation you would probably want to install it entirely.

Centreon (the company behind the Centreon software suite) provides binary packages for RedHat / CentOS. They are available either as part of the Centreon Entreprise Server distribution or as individual packages on our RPM repository.

Once the repository installed a simple command will be needed to install Centreon Broker.

$# yum install centreon-broker
Fetching sources
Beware that the repository hosts in-development sources and that it might not work at all.

Stable releases are available as gziped tarballs on Centreon's download site.

Compilation
Warning: Centreon Collect contains low-levels components of the Centreon software suite. If this is your first installation you would probably want to install it entirely.

This paragraph is only a quickstart guide for the compilation of Centreon Collect. For a more in-depth guide with build options you should refer to the online documentation.

First of all, check if you have these packages installed (Note that packages names come from Centos 7 distribution, so if some packages names don't match on your distribution try to find their equivalent names) :

git, make, cmake, gcc-c++, python3, python3-pip3, rrdtool-devel, gnutls-devel.
If they are not installed, please install them.

If you are on a Centos 7 distribution, follow these steps :

$> git clone https://github.com/centreon/centreon-broker
$> cd centreon-broker && ./cmake.sh
$> cd build
$> make & make install
If you are on an other distribution, then follow the steps below.

For the projet compilation you need to have conan installed. To install conan you need pip3 if you are using python3 (python package manager). You can install conan like that.

$> pip3 install conan
All the dependencies pulled by conan are located in conanfile.txt. If you want to use a dependency from your package manager instead of conan, you need to remove it from conanfile.txt. Then you have to add a remote conan repository, for that enter the command:

$> conan remote add centreon https://api.bintray.com/conan/centreon/centreon
Now, the command conan remote list should list two repositories:

conan-center: https://conan.bintray.com [Verify SSL: True]
centreon: https://api.bintray.com/conan/centreon/centreon [Verify SSL: True]
Once the sources of Centreon Broker extracted, execute the following commands:

$> git clone https://github.com/centreon/centreon-broker 
$> cd centreon-broker
$> mkdir build
$> cd build
$> conan install --remote centreon --build missing ..
We are adding --build missing parameter because you may have Conan complaining about missing binaries like fmt or spdlog. So with this parameter, Conan will normally install the missing binaries. Once those libraries built, always from the build directory, enter this command (Note that these cmake parameters are strongly recommended but you can choose your own) :

$> cmake -DCMAKE_BUILD_TYPE=Release -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER=centreon-broker -DWITH_GROUP=centreon-broker -DWITH_CONFIG_PREFIX=/etc/centreon-broker  \ 
         -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF=/etc/centreon-broker -DWITH_PREFIX_LIB=/usr/lib64/nagios -DWITH_MODULE_SIMU=On ..
...
Now launch the compilation using the make command and then install the software by running make install as priviledged user.

$> make 
...
$> make install
Normally if all compiles, you have finished installing broker. But if you want, you can also check it. Always from the build directory you can execute these commands :

$> bin/clib-ut
$> bin/cbwd-ut
$> bin/ccb-ut
$> bin/cce-ut

You're done !

Bug reports / Feature requests
The best way to report a bug or to request a feature is to open an issue in GitHub's issue tracker.

Please note that Centreon Broker follows the same workflow as Centreon to process issues.

For a quick resolution of a bug your message should contain :

the problem description
precise steps on how to reproduce the issue (if you're using Centreon web UI tell us where you click)
the expected behavior
the Centreon products versions
the operating system you're using (name and version)
if possible configuration, log and debug files
Contributing
Contributions are much welcome ! If possible provide them as pull-requests on GitHub. If not, patches will do but describe against which version/commit they apply.

For any question or remark feel free to send a mail to the project maintainer : Matthieu Kermagoret (mkermagoret@centreon.com).
