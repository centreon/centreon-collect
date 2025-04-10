# Centreon Collect                                                                 
                                                                                   
Centreon Collect is a collection of softwares:                                                                                     
  * [Centreon Engine](#centreon-engine)                                                         
  * [Centreon Broker](#centreon-broker)                                                            
  * [Centreon Connector](#centreon-connector) 
  * [Centreon Monitoring Agent](#centreon-monitoring-agent-cma)      
                                                   
Centreon Collect brings also [Centreon tests](centreon-tests/README.md) to test these softwares.

## Centreon Engine                                                                 
                                                                                   
Centreon Engine is a fast and powerful open-source monitoring scheduler.           
It is a low-level component of the                                                 
[Centreon software suite](https://www.centreon.com).                               
                                                                                   
Centreon Engine is released under the General Public License version 2             
and is endorsed by the [Centreon company](https://www.centreon.com).               
                                                                                   
This project was started as a fork of Nagios, the well known open-source           
monitoring application. While keeping its configuration file format and            
its stability we improved it in several ways:                                      
                                                                                   
- Reduced startup time                                                             
- Faster standard check execution engine                                           
- New light check execution system (connectors)                                    
- On-the-fly configuration reload                                                  
- Less obscure configuration options                                               
- Frequent bugfix releases                                                         
                                                                                   
Just give it a try!                                                                
                                                                                   
## Centreon Broker                                                                  
                                                                                   
Centreon Broker is an extensible open-source monitoring event                      
transmitter (broker). It is a low-level component of the                           
[Centreon software suite](https://www.centreon.com).                               
                                                                                   
Centreon Broker is released under the Apache License, Version 2.0                  
and is endorsed by the [Centreon company](https://www.centreon.com).               
                                                                                   
Centreon Broker is the communication backbone of the Centreon software             
suite so most events are processed by one or more of its module.                   
Centreon Broker has multiple modules that perform specific tasks. The              
list below describes the most common of them.                                      
                                                                                   
- SQL: store real-time monitoring events in a SQL database                         
- storage: parse and store performance data in a SQL database                      
- RRD: write RRD graph files from monitoring performance data                      
- BAM: compute Business Activity status and availability                           
- Graphite: write monitoring performance data to Graphite                          
- InfluxDB: write monitoring performance data to InfluxDB                          
                                                                                   
Centreon Broker is extremely fast and is a credible alternative to the             
old NDOutils. It is also extremly modular and can fit most network                 
security requirements. Just give it a try!                                         
                                                                                   
## Centreon Connector                                                             
                                                                                   
Centreon Connector is extremely fast open-source monitoring check                
execution daemons designed to work with [Centreon Engine](https://github.com/centreon/centreon-collect).                    
                                                                                   
It is a low-level component of the                                                 
[Centreon software suite](https://www.centreon.com).                               
                                                                                   
Centreon Connector is released under the Apache Software License version 2       
and is endorsed by the [Centreon company](https://www.centreon.com).               
                                                                                   
There are currently two open-source connectors :                                   
                                                                                   
- **Centreon Connector Perl** : persistent Perl interpreter that                   
  executes Perl plugins very fast                                                  
- **Centreon Connector SSH** : maintain SSH connexions opened to reduce            
  overhead of plugin execution over SSH     

## Centreon Monitoring Agent (CMA)

The **Centreon Monitoring Agent** is a lightweight, asynchronous service designed to monitor system health and metrics on Windows and Linux hosts degined to work with [Centreon Engine](https://github.com/centreon/centreon-collect).

More information [CMA](https://github.com/centreon/centreon-collect/blob/develop/agent/doc/agent-doc.md)

## Documentation

*https://docs.centreon.com*

## Installing from binaries

> Centreon Collect is a low-level component of the Centreon
> software suite. If this is your first installation you would probably
> want to [install it entirely](https://docs.centreon.com/current/en/installation/installation-of-a-central-server/using-sources.html).

Centreon ([the company behind the Centreon software suite](http://www.centreon.com))
provides binary packages for RedHat / CentOS. They are available either
as part of the [Centreon Platform](https://www.centreon.com/en/platform/)
or as individual packages on [our RPM repository](https://docs.centreon.com/current/en/installation/installation-of-a-poller/using-packages.html).

Once the repository installed a simple command will be needed to install
Centreon Collect.

```shell
yum install centreon-broker centreon-clib centreon-connector centreon-engine
```

## Fetching sources

Beware that the repository hosts in-development sources and that it
might not work at all.

Stable releases are available as gziped tarballs on [Centreon's
download site](https://download.centreon.com).

## Compilation

This paragraph is only a quickstart guide for the compilation of
Centreon Collect.

### CentOS / Debian / Raspbian / Ubuntu

Compilation of these distributions is pretty straightforward.

#### Packages to install

On Debian/Raspbian/Ubuntu, you should install these packages:

```shell
apt install librrd-dev libgnutls28-dev liblua5.3-dev libperl-dev libgcrypt20-dev libssl-dev libssh2-1-dev zlib1g-dev libcurl4-openssl-dev
```

On CentOS, you should install these packages:

```shell
dnf install gnutls-devel libgcrypt-devel lua-devel rrdtool-devel selinux-policy-devel openssl-devel libssh2-devel libcurl-devel zlib-devel
```

And you'll probably need to enable some repos for the installation to succeed, for example:

```shell
dnf config-manager --set-enabled powertools
dnf update
```

#### Compilation

You'll need to download the project and launch the *cmake-vcpkg.sh* script to prepare the compilation environment:

```shell
git clone https://github.com/centreon/centreon-collect
cd centreon-collect
./cmake-vcpkg.sh
```

Now launch the compilation using the *make* command and then install the
software by running *make install* as priviledged user:

```shell
make -Cbuild
make -Cbuild install
```

**Remark. ** The cmake-vcpkg.sh initializes two environment variables like this:
* `VCPKG_ROOT=`*path to centreon-collect*`/vcpkg`
* `PATH=$VCPKG_ROOT:$PATH`

These two variables are very important if you want to recompile the project later.


### Other distributions

If you are on another distribution, then follow the steps below.

Check if you have these packages installed (Note that packages names
come from CentOS distributions, so if some packages names don't match
on your distribution try to find their equivalent names): git, make, cmake, gcc-c++, python3, libgnutls-devel, liblua-devel, librrd-devel, openssl-devel, zlib-devel, libcurl-devel, libssh2-devel.

You can now prepare the compilation environment:

```shell
git clone https://github.com/centreon/centreon-collect
cd centreon-collect
export VCPKG_ROOT=$PWD/vcpkg
export PATH=$VCPKG_ROOT:$PATH
git clone -b 2024.01.12 https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
mkdir build
cmake -B build \
      -DVCPKG_OVERLAY_TRIPLETS=custom-triplets \
      -DVCPKG_TARGET_TRIPLET=x64-linux-release \
      -DVCPKG_OVERLAY_PORTS=overlays \
      -DWITH_TESTING=On \
      -DWITH_CREATE_FILES=OFF \
      -DWITH_CONF=OFF \
      -S .
```

This will look for required dependencies and print a summary of the
compilation parameters if everything went fine.

Now launch the compilation using the *make* command and then install the
software by running *make install* as priviledged user:

```shell
make -Cbuild
make -Cbuild install
```

Normally if all compiles, you have finished installing Collect. But if
you want, you can also check it. From the *build* directory you
can execute these commands:

```shell
tests/ut_broker
tests/ut_engine
```

You're done!

### Inside a docker, ubuntu for example
In my case I have the home directory of the centreon project in /data/dev/centreon-collect

First create the ubuntu container and jump into
```shell
docker container run --name ubuntu22.04 -ti -v /data/dev/centreon-collect:/root/centreon-collect  ubuntu:22.04 /bin/bash
```
Now you are in your ubuntu container
```shell
cd /root/centreon-collect/
apt install librrd-dev libgnutls28-dev liblua5.3-dev libperl-dev libgcrypt20-dev libssl-dev libssh2-1-dev zlib1g-dev libcurl4-openssl-dev cmake
./cmake-vcpkg.sh 
make -Cbuild
make -Cbuild install
```
Then you have all binaries compiled in the ubuntu distribution.
You can access it outside the container in the /data/dev/centreon-collect/build directory

### Windows 
 Only the Centreon Monitoring Agent and the CMA installer can be compiled on Windows
#### Prerequisites
To compile the agent, ensure the following tools are installed::
* [Msbuild tools](https://visualstudio.microsoft.com/downloads/) install desktop development with C++
* [Git](https://git-scm.com/downloads/win)
* [NSIS](https://sourceforge.net/projects/nsis/files/NSIS%202/)
* [PowerShell Core](https://github.com/PowerShell/PowerShell/releases)
#### Compiling the Agent
Then you have to:
* Add NSIS path to environment variables
* Open x64 Native Tools Command Prompt for VS
```shell
cd centreon-collect/
centreon_cmake.bat
 ```
 When Executing centreon_cmake.bat. It first installs vcpkg in your home directory and then Prompt you to set two environment variables: VCPKG_ROOT and PATH. 
> ⚠️ **Warning:** However, be aware that each time you open a new x64 Native Tools Command Prompt, it may set VCPKG_ROOT to an incorrect value by default.
You will need to manually reset VCPKG_ROOT to the correct path before building again

* Then install agent\conf\agent.reg in the registry and modify parameters such as server, certificates or logging.

!!! note  
    To compile the agent with the installer add the option : DWITH_BUILD_AGENT_INSTALLER=On

## Bug reports / Feature requests

The best way to report a bug or to request a feature is to open an issue
in GitHub's [issue tracker](https://github.com/centreon/centreon-collect/issues/).

Please note that Centreon Collect follows the
[same workflow as Centreon](https://github.com/centreon/centreon/issues/new/choose)
to process issues.

For a quick resolution of a bug your message should contain:

- The problem description
- Precise steps on how to reproduce the issue (if you're using Centreon
  web UI tell us where you click)
- The expected behavior
- The Centreon product**s** version**s**
- The operating system you're using (name and version)
- If possible configuration, log and debug files

## Contributing

Contributions are much welcome! If possible provide them as
pull-requests on GitHub. If not, patches will do but describe against
which version/commit they apply.

For any question or remark feel free to send a mail to the project
maintainers:

<a href="https://github.com/bouda1"><img src="https://avatars1.githubusercontent.com/u/6324413?s=400&v=4" title="David Boucher" width="80" height="80"></a> &nbsp;
<a href="https://github.com/jean-christophe81"><img src="https://avatars.githubusercontent.com/u/98889244?v=4" title="Jean-Christophe Roques" width="80" height="80"></a> &nbsp;
