# Engine documentation {#mainpage}

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
