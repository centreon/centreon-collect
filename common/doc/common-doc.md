# Common documentation {#mainpage}

## Table of content

* [Pool](#Pool)
* [Grpc](#Grpc)


## Pool

After a fork, only caller thread is activated in child process, so we mustn't join others. That's why thread container is dynamically allocated and not freed in case of fork.


## Grpc
The goal of the two classes provided, grpc_server_base and grpc_client_base is to create server or channel in order to use it with service. 
grpc_server_base creates a ::grpc::server object. You can register service with third constructor parameter builder_option.
grpc_client_base creates a ::grpc::channel that can be used to create a stub.