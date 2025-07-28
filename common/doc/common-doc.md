# Common documentation {#mainpage}

## Table of content

* [Pool](#Pool)
* [Grpc](#Grpc)
* [Process](#Process)


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

## Process

The goal of this class is to provide an base class to execute asynchronously process according to asio library.
It relies on boost v2 process library.
All is asynchronous, child process end of life is notified to on_process_end method. It's the same for stdin write and stdout/err read.

You have 3 constructors that allow user to pass executable arguments in four different ways. One of them accept a string command line with exe and arguments

When you have to start several times the same process, the better way is to create a shared_ptr< std::string > (exe path) and a shared_ptr of string vector for arguments with parse_cmd_line static method. Then, you can pass it to this constructor:
```c++
  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const shared_str& exe_path,
          bool use_setpgid,
          bool use_stdin,
          const shared_args& args,
          const shared_env& env);

```

This class can be used only once. 

The `process` class can be used alone to execute a program directly in an asynchron way.

Here is another practical example:

```
#include "com/centreon/common/process/process.hh"

void do_stuff() {
  /* process must be a shared_ptr. */
  auto p = std::make_shared<process<false>>(g_io_context, _logger,
                            "/usr/bin/perl " HTTP_TEST_DIR "/vault-server.pl");

  /* Here the process is started. */
  p->start_process(false);

  /* Now, while the process is running, we can do our work. */
  my_function_that_exchanges_with_process();
  my_other_function_that_doesnt_work_with_process();

  /* When all the stuff is done, we can stop p */
  p->kill();
}
```

## Vault

Broker can work with Hashicorp Vault to get its database passwords.
We have a library designed for that in `common/vault`.

To use it, we need two files:
* the JSON vault file that contains all the fields needed to access the Vault.
* an env file that contains the APP_SECRET salt used to encrypt the Vault access secrets.

The Vault file is of the form:
```
{
  "name": "my_vault",
  "url": "localhost",
  "port": 4443,
  "root_path": "john-doe",
  "secret_id": "{your_secret_key}",
  "role_id": "{your_role_id}",
  "salt": "{your_salt}"
}
```

The `secret_id` and the `role_id` are used for the authentication to the Vault. They are AES256 encrypted
in this file so not directly usable.
The `salt` is used during the AES256 encryption, `url` and `port` are the access to the vault.

If we have these two files and a spdlog::logger, it is pretty simple to access the vault.

Let's suppose we have a path in the vault and we want to get the corresponding password, let's say
```
std::string path = "secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password";
```

We can use the following code to get the password:

```
std::string env_file("/tmp/env_file");
std::string vault_file("/tmp/vault_file");
bool verify_peer = true;
std::shared_ptr<spdlog::logger> logger = my_logger();
common::vault::vault_access vault(env_file, vault_file, verify_peer, logger);
std::string password = vault.decrypt(path);
```

In case of error, an exception is thrown with the error message, so to also catch the
message we can write something like this:

```
std::string env_file("/tmp/env_file");
std::string vault_file("/tmp/vault_file");
bool verify_peer = true;
std::shared_ptr<spdlog::logger> logger = my_logger();
try {
  common::vault::vault_access vault(env_file, vault_file, verify_peer, logger);
  std::string password = vault.decrypt(path);
} catch (const std::exception& e) {
  logger->error("Error with the vault: {}", e.what());
}
```

### Asio bug work around
There is an issue in io_context::notify_fork. Internally, ctx.notify_fork calls epoll_reactor::notify_fork which locks registered_descriptors_mutex_. An issue occurs when registered_descriptors_mutex_ is locked by another thread at fork timepoint. 
In such a case, child process starts with registered_descriptors_mutex_ already locked and both child and parent process will hang.

