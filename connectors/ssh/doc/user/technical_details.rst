#################
Technical details
#################

This article describes how Centreon SSH Connector allow much gain on SSH
check execution.

One major CPU-intensive and long operation in a SSH environment is the
key exchange and verification mechanism. This operation occurs when a
SSH session is started between two hosts. After this step all exchange
operations are using far less resources.

Centreon SSH Connector take advantage of this fact and maintain
semi-permanent connection with hosts to which it had to connect to. This
way if multiple checks are performed on the same host, where
"check_by_ssh" opens one session for each check, Centreon Connector
SSH only opens one session. However this does not limit the number of
concurrent checks on a host, as the SSH protocol allows multiple
channels to be opened on the same session. Therefore if multiple checks
are run on the same host simultaneously, they are executed concurrently
but with separate execution environment.
