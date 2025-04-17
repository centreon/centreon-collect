
## Gorgone log syncronization with pullwss communcation mode.


each box represent a process running. Each participant represent a file used by the process.\
as participant name must be unique, P or C represent if the file is used on the Poller or the Central\
dotted line are zmq message, full line are direct subroutine call.

### Pullwss
Please note that pullwss communication mode have a limitation on the message size that zmq don't have.
To limit the size of the GETLOG message, the pullwss module split the message in smaller part, and send multiple SETLOGS message.
Other communication module send only one SETLOGS message in this case.

```mermaid
sequenceDiagram
    # title: 

    box rgb(100,100,102) central-core
        participant C/class/core
        participant C/proxy/hook
    end
    box rgb(100,100,102) central-proxy-httpserver
        participant C/class/module
        participant C/proxy/httpserver
    end

    box rgb(100,100,102) poller-pullwss
        participant P/pullwss/class
        participant P/class/module
    end
    box rgb(100,100,102) poller-core
        participant P/class/core
        participant P/library
    end

    C/class/core -> + C/class/core: run
    C/class/core -> + C/class/core: EV::timer(every 5s)
    C/class/core -> + C/class/core: periodic_exec
    C/class/core -> + C/class/core: check_exit_modules
    C/class/core ->> + C/proxy/hook: check
    C/proxy/hook -> + C/proxy/hook: full_sync_history
    note left of C/proxy/hook: full_sync_history is called<br/> every synchistory_time <br/>sec, see proxy.md
    C/proxy/hook -> + C/proxy/hook: routing
    C/proxy/hook -> C/proxy/hook: get_sync_time
    C/proxy/hook -> C/proxy/hook: send_internal_message
    C/proxy/hook --) + C/proxy/httpserver: GETLOG
    deactivate C/proxy/hook
    deactivate C/proxy/hook
    deactivate C/proxy/hook
    deactivate C/class/core
    deactivate C/class/core
    deactivate C/class/core
    deactivate C/class/core

# central use the proxy module to send data to the remote node. proxy have multiples process runing in parallel,
# it is not clear for now if the main process see message before the worker process.
    C/proxy/httpserver ->  + C/proxy/httpserver: read_zmq_events
    C/proxy/httpserver ->  + C/proxy/httpserver: proxy
    C/proxy/httpserver --) P/pullwss/class: GETLOGS
    deactivate C/proxy/httpserver
    deactivate C/proxy/httpserver

# the poller (which is a distant node) retrieve the GETLOG message, and process it.
# first the pullwss module listen the websocket, and transmit the message retrieved to the core.
# the core process it and send back the response.
# TODO : make a note wss_connect is not called everytime.
    P/pullwss/class -> + P/pullwss/class: wss_connect
    P/pullwss/class ->> + P/class/module: send_internal_action
    P/class/module --) P/class/core: GETLOG
    P/class/core -> + P/class/core: router_internal_event
    P/class/core -> + P/class/core: message_run
    P/class/core ->> P/library: getlog
    P/class/core ->  + P/class/core: send_internal_response
    P/class/core --) P/pullwss/class: GETLOG
    deactivate P/class/core
    deactivate P/class/core
    deactivate P/class/core


    # on the distant poller, processing of the message.
    P/pullwss/class -> + P/pullwss/class: read_zmq_events
    P/pullwss/class -> + P/pullwss/class: transmit_back
    loop split the logs in smaller message if it's too big for the websocket, making possibility multiples SETLOG message
        P/pullwss/class -> + P/pullwss/class: send_message
        P/pullwss/class --) C/proxy/httpserver: SETLOGS
        deactivate P/pullwss/class
        deactivate P/pullwss/class
        deactivate P/pullwss/class

        # on the central process the response
        C/proxy/httpserver -> + C/proxy/httpserver: read_message_client
        C/proxy/httpserver ->> + C/class/module: send_internal_action
        C/class/module --) C/class/core: SETLOGS
        C/class/core -> + C/class/core: router_internal_event
        C/class/core -> + C/class/core: message_run
        C/class/core ->> + C/proxy/hook: routing
        C/proxy/hook -> C/proxy/hook: setlogs
        note left of C/proxy/hook: setlogs is charged to <br/>insert data in the database

        deactivate C/proxy/hook
        deactivate C/class/core
        deactivate C/class/core
    end



``` 