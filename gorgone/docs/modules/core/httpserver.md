# HTTP Server

## Description

This module aims to provide a HTTP/S server to expose handy endpoints to talk to Gorgone.

It relies on a core API module to server Gorgone events and can dispatch any other piece of code.

## Configuration

| Directive               | Description                                                                | Default value |
|:------------------------|:---------------------------------------------------------------------------|:--------------|
| address                 | IP address for the server to bind to                                       | `0.0.0.0`     |
| port                    | Port on which the server will listen to requests                           | `8080`        |
| ssl                     | Boolean to enable SSL terminaison                                          | `false`       |
| ssl_cert_file           | Path to the SSL certificate (if SSL enabled)                               |               |
| ssl_key_file            | Path to the SSL key (if SSL enabled)                                       |               |
| auth                    | Basic credentials to access the server                                     |               |
| allowed_hosts           | Peer address to access the server                                          |               |
| no_shell_interpretation | boolean allowing shell interpolation of command sent to the action module. | true          |

if `no_shell_interpretation` is set to `0` or `false`, the command sent to the action module will be interpreted by the shell. 
This can be useful for some commands but can also be a security risk. By default, the value is `true` to avoid any shell interpretation.

#### Example

```yaml
name: httpserver
package: "gorgone::modules::core::httpserver::hooks"
enable: true
address: 0.0.0.0
port: 8443
ssl: true
ssl_cert_file: /etc/pki/tls/certs/server-cert.pem
ssl_key_file: /etc/pki/tls/server-key.pem
auth:
  enabled: true
  user: admin
  password: password
allowed_hosts:
  enabled: true
  subnets:
    - 127.0.0.1/32
    - 10.30.2.0/16
```

Below the configuration to add other endpoints:

```yaml
dispatch:
  - endpoint: "/mycode"
    method: GET
    class: "path::to::my::code"
```

## Events

| Event           | Description                       |
| :-------------- | :-------------------------------- |
| HTTPSERVERREADY | Internal event to notify the core |
