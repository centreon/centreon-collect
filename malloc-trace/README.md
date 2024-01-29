# malloc-trace

## Description

The goal of this little library is to trace each orphan malloc free call. It overrides weak **malloc**, **realloc**, **calloc** and **free**
We store in a container in memory every malloc, free calls. We remove malloc from that container each time a free with the same address is called otherwise free is also store in the container.
Every minute (by default), we flush to disk container content:
 * malloc that had not be freed and that are older than one minute
 * free that has not corresponding malloc in the container.

In order to use it you have to feed LD_PRELOAD env variable
```bash
export LD_PRELOAD=malloc-trace.so
```
Then you can launch your executable and each call will be recorded in /tmp/malloc-trace.csv with ';' as field separator

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
f field is function name
s field is source file if available
l field is source line

This library works in that manner:
We replace all malloc, realloc, calloc and free in order to trace all calls.
We store all malloc in a container. Each time free is called, if the corresponding malloc is found, it's erased from container, 
otherwise orphan free is stored in the container.
Every 60s (by default), we flush the container, all malloc older than 60s and not freed are dumped to disk, all orphan freed are also dumped.

Output file may be moved during running, in that case it's automatically recreated.

## Environment variables
Some parameters of the library can be overriden with environment variables.
| Environment variable     | default value         | description                                                                   |
| ------------------------ | --------------------- | ----------------------------------------------------------------------------- |
| out_file_path            | /tmp/malloc-trace.csv | path of the output file                                                       |
| out_file_max_size        | 0x100000000           | when the output file size exceeds this limit, the ouput file is truncated     |
| malloc_second_peremption | one minute            | delay between two flushes and delay after which malloc is considered orphaned |

## Provided scripts

### create_malloc_trace_table.sql
This script creates a table that can store an output_file
In this scripts, you will find in comments how to load output csv file in that table.

### remove_malloc_free.py
We store in output file malloc that aren't freed in the next minute, we also store orphan free.
So if a malloc is dumped and it's corresponding free is operated two minutes later, the two are stored in output file.
The purpose of this script is to remove these malloc-free pairs.
