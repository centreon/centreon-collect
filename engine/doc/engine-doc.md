# Engine documentation {#mainpage}

## Whitelist (since 23.10)

In order to enforce security, user can add a whitelist to centreon-engine.
When the user add a file in /etc/centreon-engine-whitelist or in /usr/share/centreon-engine-whitelist, centengine only executes commands that match to the expressions given in these files.
Beware, Commands are checked after macros replacement by values, the entire line is checked, the script and his arguments.

### whitelist format
whitelist files must contain regular expressions that will filter commands. An OR is made on expressions, if a command matches with one expression, it's allowed.
#### yaml
```yaml
whitelist:
  regex:
    - \/usr\/lib64\/nagios\/plugins\/check_icmp.*
    - \/usr\/lib\/centreon\/plugins\/centreon_linux_snmp\.pl.*
```
#### json
```json
{
    "whitelist":{
        "regex":[
            "/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"
        ]
    }
}
```

The main main job is done by whitelist class, it parses file and compares final commands to regular expressions
This class is a singleton witch is replace by another instance each time conf is reloaded.

Checkable class inherited by service and host classes keeps the last result of whitelist's check in cache in order to reduce CPU whitelist usage.


## Engine docker image

Each time develop branch of collect is compiled several engine docker images are created: docker.centreon.com/centreon/centreon-collect-engine-....
In this image, you will find of course centengine with default connection, but also cbmod, connectors, tcp and lua module, nagios plugins and dependencies needed by centreon perl plugins but no perl plugins. 

centengine is started by engine/scripts/launch-engine-in-container.sh script. It creates all mandatory folders like /var/log/centreon-engine with the correct rights

### Directories mapping
| container directory       | description                                                                                                             |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| /etc/centreon-engine      | engine configuration (overrides default engine configuration)                                                           |
| /etc/centreon-broker      | cbmod configuration (overrides default module configuration)                                                            |
| /var/log                  | container will create centreon-engine and centreon-broker to write logs of cbmod and centengine                         |
| /var/lib                  | container will create centreon-engine and centreon-broker to store queue files, stats and command pipe (centengine.cmd) |
| /usr/lib/centreon/plugins | directory where centengine will find perl plugins (no plugins is provided by the container)                             |


### Run with default configuration
Default configuration contains four hosts with one ping service.
You have to provide broker host address as a param to docker run.
The default module configuration has broker host 'broker_host' that will be replaced by docker run first argument.

Example to connect engine to a broker running on parent host:
```bash
docker run  -v /data/engine_folders/lib:/var/lib -v /data/engine_folders/log:/var/log -v /data/engine_folders/centron-plugins:/usr/lib/centreon/plugins docker.centreon.com/centreon/centreon-collect-engine-alma8-amd64:develop 172.17.0.1
```

### Provide your own configuration

In order to use it, you must provides:
* a directory that contains engine configuration that must be mapped in /etc/centreon-engine
* a directory that contains module configuration that must be mapped in /etc/centreon-broker
* a directory to get engine and module logs that must be mapped in /var/log
* a directory to map to /var/lib, by this you will be able to access centengine.cmd pipe
* a directory that contains centreon perl plugins that mus be mapped to /usr/lib/centreon/plugins

An example:

On host hard drive:
/data/engine_folders/
  * centreon-engine
    * centengine.cfg
    * commands.cfg
    * connectors.cfg
    * hosts.cfg
    * services.cfg
    * timeperiods.cfg
    * ....
  * centreon-broker
    * central-module.json
  * centron-plugins
    * centreon_linux_snmp.pl
    * centreon_linux_local.pl
    * centreon_protocol_http.pl
    * .....
  * lib
  * log
  
Then you execute:
```bash
docker run -v /data/engine_folders/centreon-engine:/etc/centreon-engine -v /data/engine_folders/centreon-broker:/etc/centreon-broker -v /data/engine_folders/lib:/var/lib -v /data/engine_folders/log:/var/log -v /data/engine_folders/centron-plugins:/usr/lib/centreon/plugins docker.centreon.com/centreon/centreon-collect-engine-alma9-amd64:develop
```

Then you will find logs in /data/engine_folders/log/centreon-engine/centengine.log and /data/engine_folders/log/centreon-broker/central-module.log
You will have also access to centengine.cmd by /data/engine_folders/lib/centreon-engine/rw/centengine.cmd
```bash
echo "[1703149790] SCHEDULE_FORCED_HOST_CHECK;hostzezrezrz_50000;1703149790" >> /data/engine_folders/lib/centreon-engine/rw/centengine.cmd
``` 
Lua module and lua interpreter are provided in order that you can use lua output for automatic tests for example.