# Pullwss

## Description

This module should be used on remote nodes where the connection has to be HTTP/HTTPS and must be opened from the node to the Central Gorgone.

This module requires proxy and register module to be configured on the central Gorgone.
The register Module will allow Gorgone to keep the state of every poller, and find out the connection mode. 
The proxy module has to bind to a tcp port for the pullwss module to connect to.

## Configuration

| Directive    | Description                                        | Default value |
|:-------------|:---------------------------------------------------|:--------------|
| ssl          | should the connection be made over TLS/SSL or not  | `false`       |
| address      | IP address to connect to                           |               |
| port         | TCP port to connect to                             |               |
| token        | token to authenticate to the central gorgone       |               |
| proxy        | HTTP(S) proxy to access central gorgone            |               |
| max_msg_size | max message size to send to the central Gorgone    | `130000`      |
### Example

```yaml
name: pullwss
package: "gorgone::modules::core::pullwss::hooks"
enable: true
ssl: true
port: 8086
token: "1234"
address: 192.168.56.105
```

## Events

| Event          | Description                                             |
|:---------------|:--------------------------------------------------------|
| PULLWSSREADY   | Internal event to notify the core this module is ready. |

## API

No API endpoints.

Here the visual representation of the start of the central gorgone, it's interaction with the database, and the authentication process of the pullwss module.
There is one box per server and a participant per process in gorgone in the diagram below.
```mermaid
sequenceDiagram
    # title: 

    box rgb(100,100,102) central-configuration
        
    end
    box rgb(100,100,102) central
        participant C/nodes
        participant C/core
        participant C/proxy/httpserver
    end
    
    box rgb(100,100,102) poller
        participant P/pullwss
        participant P/core
    end


    C/core -> + C/core: run
    C/core -> + C/core: init
    note left of C/core: main process read the yaml configuration file at boot once, then load modules specified, <br/>and pass each module a copy of the core configuration and it's specific configuration.
    C/core -> + C/core: load_modules
    deactivate C/core
    deactivate C/core
    deactivate C/core
    C/core -> + C/nodes: init
    C/nodes -> + C/nodes: periodic_exec
    C/nodes -> + C/nodes: action_centreonnodessync
    note left of C/nodes: the nodes module will periodically (and when asked by api) <br/>query every poller info in the centreon database, and send the result to the other modules (like proxy) <br/>to keep the state of every poller up to date.
    deactivate C/nodes
    deactivate C/nodes
    C/nodes --) C/core: REGISTERNODES
    note left of C/core: the code is in proxy/hooks.pm, but it's the core process that execute the following code.
    C/core -> + C/core: proxy/hooks.pm:routing
    C/core -> + C/core: proxy/hooks.pm:register_nodes
    note left of C/core: nodes info will be stored in process memory (variable register_nodes and some others)
    C/core --) C/proxy/httpserver: PROXYADDNODE
    C/proxy/httpserver -> + C/proxy/httpserver: action_proxyaddnode
    note left of C/proxy/httpserver: the httpserver module will store the node list, and try to disconnect nodes that are not in the list anymore.<br/>At this point the central is ready to accept poller connections.
    
    P/core -> + P/core: run
    P/core -> + P/core: init
    P/core -> + P/core: load_modules
    P/core ->> + P/pullwss: run
    P/pullwss -> + P/pullwss: wss_connect
    note left of P/pullwss: the pullwss module will try to connect to the central gorgone using the yaml configuration, sending the token as an header. the id is not sent at this step.
    P/pullwss -> + P/pullwss: ping
    note left of P/pullwss: the registernode message contain the id of the node, which will be used by the central to properly authenticate the poller.
    P/pullwss --) C/proxy/httpserver: REGISTERNODES
    C/proxy/httpserver -> + C/proxy/httpserver: websocket message handler (start of the file, before functions).
    C/proxy/httpserver -> + C/proxy/httpserver: is_logged_websocket
    C/proxy/httpserver -> + C/proxy/httpserver: is_token_ok
    note left of C/proxy/httpserver: once the connection is deemed authenticated, <br/>the httpserver module will transmit the REGISTERNODES message to the core, <br/>which will update the node state. Every message received on the websocket after authentication<br/>will be transmitted to the core, and the core will decide what to do with it.
    C/proxy/httpserver --) C/core: REGISTERNODES
    C/core -> + C/core: proxy/hooks.pm:routing
    C/core -> + C/core: proxy/hooks.pm:register_nodes

    
    
    
    

    
    
    
```
