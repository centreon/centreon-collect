# Common documentation {#mainpage}

## Table of content

* [Pool](#Pool)


## Pool

After a fork, only caller thread is activated in child process, so we mustn't join others. That's why thread container is dynamically allocated and not freed in case of fork.
