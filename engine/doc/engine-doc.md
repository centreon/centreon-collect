# Engine documentation {#mainpage}

## Whitelist (since 23.10)

In order to enforce security, user can add a whitelist to centreon-engine.
When the user add a file in /etc/centreon-engine-whitelist or in /usr/share/centreon-engine/whitelist.conf.d, centengine only executes commands that match to the expressions given in these files.
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

## Extended configuration
Users can pass an additional configuration file to engine. Gorgone is not aware of this file, so users can override centengine.cfg configuration.
Each entry found in additional json configuration file overrides its twin in `centengine.cfg`.

### examples of command line
```sh
/usr/sbin/centengine --config-file=/tmp/centengine_extend.json /etc/centreon-engine/centengine.cfg

/usr/sbin/centengine --c /tmp/file1.json  --c /tmp/file2.json /etc/centreon-engine/centengine.cfg
```

In the second case, values of file1.json will override values of centengine.cfg and values of file2.json will override values of file1.json

### file format
```json
{   
    "send_recovery_notifications_anyways": true
}
```

### implementation detail
In `state.cc` all setters have two methods:
* `apply_from_cfg`
* `apply_from_json`.

On configuration update, we first parse the `centengine.cfg` and all the `*.cfg` files, and then we parse additional configuration files.
