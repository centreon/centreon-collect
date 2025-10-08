# Action

## Description

This module aims to execute actions on the server running the Gorgone daemon or remotly using SSH.

## Configuration

| Directive        | Description                                                    | Default value |
|:-----------------|:---------------------------------------------------------------|:--------------|
| command_timeout  | Time in seconds before a command is considered timed out       | `30`          |
| whitelist_cmds   | Boolean to enable commands whitelist                           | `false`       |
| allowed_cmds     | Regexp list of allowed commands                                |               |
| paranoid_plugins | Block centengine restart/reload if plugin dependencies missing | `false`       |

#### Example

```yaml
name: action
package: "gorgone::modules::core::action::hooks"
enable: true
command_timeout: 30
whitelist_cmds: true
allowed_cmds: !include whitelist.conf.d/*.yaml
```

Allowed_cmds use a dedicated directory containing by one file provided by Centreon. 
Other whitelist should be set in this directory in a new file to avoid conflict on update.

## Events

| Event       | Description                                                                             |
|:------------|:----------------------------------------------------------------------------------------|
| ACTIONREADY | Internal event to notify the core                                                       |
| PROCESSCOPY | Process file or archive received from another daemon                                    |
| COMMAND     | Execute a shell command on the server running the daemon or on another server using SSH |

## API

### Execute a command line

| Endpoint             | Method |
|:---------------------|:-------|
| /core/action/command | `POST` |

#### Headers

| Header       | Value            |
|:-------------|:-----------------|
| Accept       | application/json |
| Content-Type | application/json |

#### Body

| Key               | Value                                                    |
|:------------------|:---------------------------------------------------------|
| command           | Command to execute                                       |
| timeout           | Time in seconds before a command is considered timed out |
| continue_on_error | Behaviour in case of execution issue                     |

```json
[
    {
        "command": "<command to execute>",
        "timeout": "<timeout in seconds>",
        "continue_on_error": "<boolean>"
    }
]
```


#### Example

See a complete exemple of this endpoint in the [api documentation](../../api.md)

```bash
curl --request POST "https://hostname:8443/api/core/action/command" \
  --header "Accept: application/json" \
  --header "Content-Type: application/json" \
  --data "[
    {
        \"command\": \"echo 'Test command' >> /tmp/here.log\"
    }
]"
```
Output : 
```bash
{"token":"b3f825f87d64764316d872c59e4bae69299b0003f6e5d27bbc7de4e27c50eb65fc17440baf218578343eff7f4d67f7e98ab6da40b050a2635bb735c7cec276bd"}
```

