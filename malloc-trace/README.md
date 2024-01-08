# malloc-trace

The goal of this little library is to trace each malloc free call. It overrides weak malloc and free
In order to use it ou have to feed LD_PRELOAD env variable
```bash
export LD_PRELOAD=malloc-trace.so
```
Then you can launch you executable and each call will be recordedin /tmp/malloc-trace.csv
The columns are:
* function (malloc or free)
* thread id
* address in process mem space
* size of memory allocated
* call stack contained in a json array
    ```json
    [
        {
            "f": "free",
            "s": "",
            "l": 0
        },
        {
            "f": "__gnu_cxx::new_allocator<std::_Sp_counted_ptr_inplace<boost::asio::io_context, std::allocator<boost::asio::io_context>, (__gnu_cxx::_Lock_policy)2> >::deallocate(std::_Sp_counted_ptr_inplace<boost::asio::io_context, std::allocator<boost::asio::io_context>, (__gnu_cxx::_Lock_policy)2>*, unsigned long)",
            "s": "",
            "l": 0
        }
    ]
    ```