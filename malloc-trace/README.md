# malloc-trace

## Description

The goal of this little library is to trace each orphan malloc free calls. It overrides weak **malloc**, **realloc**, **calloc** and **free**
In order to use it ou have to feed LD_PRELOAD env variable
```bash
export LD_PRELOAD=malloc-trace.so
```
Then you can launch you executable and each call will be recorded in /tmp/malloc-trace.csv with ';' as field separator

The columns are:
* function (malloc or free)
* thread id
* address in process mem space
* size of memory allocated
* timestamp in ms
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

This library works in that manner:
We replace all malloc, realloc, calloc and free in order to trace all calls.
We store all malloc in a container. Each time free is called, if the corresponding malloc is found, it's erased from container, 
otherwise orphan free is stored in the container.
Every 60s (by default), we flush the container, all malloc older than 60s and not freed are dumped to disk, all orphan freed are also dumped.

Output file may be move during running, it's recreated.

## Environment variables
Some parameters of the library can be overrided with environment variable
| Environment variable     | default value         | description                                                               |
| ------------------------ | --------------------- | ------------------------------------------------------------------------- |
| out_file_path            | /tmp/malloc-trace.csv | path of the output file                                                   |
| out_file_max_size        | 0x100000000           | when the output file size exceed this limit, the touput file is truncated |
| malloc_second_peremption | one minute            | delay between 2 flush and delay after we consider malloc orphans          |

## Provided scripts

### create_malloc_trace_table.sql
This script create a table that can store an output_file
The how to load csv file in it is described in comments

### remove_malloc_free.py
We store in file malloc that aren't free in the minute after, we also store corresponding free.
So if a malloc is dumped and it's corresponding free is operated 2 minutes later, the two are stored in file.
The purpose of this script is to remove these malloc free pairs.