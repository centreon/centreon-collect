# Common documentation {#mainpage}

## Table of content

* [Pool](#Pool)
* [Grpc](#Grpc)


## Pool

After a fork, only caller thread is activated in child process, so we mustn't join others. That's why thread container is dynamically allocated and not freed in case of fork.


## Grpc
The goal of the two classes provided, grpc_server_base and grpc_client_base is to create server or channel in order to use it with grpc generated services such as exchange in broker grpc module. 
* `grpc_server_base` creates a ::grpc::server object. You can register service with third constructor parameter builder_option.
* `grpc_client_base` creates a ::grpc::channel that can be used to create a stub.

In order to use it you have to override and use protected methods and attributes:
  * server:
```c++
class my_grpc_server: public public com::centreon::common::grpc::grpc_server_base {
  std::shared_ptr<service_impl> _service;
public:
  my_grpc_server(const grpc_config::pointer& conf);
};

my_grpc_server::my_grpc_server(const grpc_config::pointer& conf)
    : com::centreon::common::grpc::grpc_server_base(conf, log_v2::grpc()) {
  _init([this](::grpc::ServerBuilder& builder) {
    _service = std::make_shared<service_impl>(
        std::static_pointer_cast<grpc_config>(get_conf()));
    builder.RegisterService(_service.get());
  });
}
```
  * client:
```c++
class my_grpc_client : public com::centreon::common::grpc::grpc_client_base {
  std::unique_ptr<com::centreon::my_service::Stub> _stub;

 public:
  my_grpc_client(const grpc_config::pointer& conf);
};

my_grpc_client::my_grpc_client(const grpc_config::pointer& conf)
    : com::centreon::common::grpc::grpc_client_base(conf, log_v2::grpc()) {
  _stub = std::move(com::centreon::my_service::NewStub(_channel));
}


```
