*** Settings ***
Documentation       check Gorgone don't leak file descriptor when a poller become unavailable

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        1200s

*** Test Cases ***
check gorgone proxy do not leak file descriptor with a poller
    [Tags]    long_tests
    [Teardown]    Stop Gorgone And Remove Gorgone Config    push_zmq_gorgone_central    sql_file=${ROOT_CONFIG}database/delete_pollers.sql
    ${cmd_count_file_descriptor}=    Set Variable    count=0; for pid in \$(ps aux | grep gorgone-proxy | grep -v grep | awk '{ print \$2 }') ; do num=\$(lsof | grep \$pid | wc -l); count=\$((count + \$num)) ; done ; echo \$count

    Log To Console    \nStarting the gorgone setup
    Setup Two Gorgone Instances    communication_mode=push_zmq     central_name=push_zmq_gorgone_central    poller_name=push_zmq_gorgone_poller_2
    # We wait for gorgone to be ready, and grab all file descriptor it need.
    Sleep    10
    ${before_kill_fd_nb}    Run    ${cmd_count_file_descriptor}
    Stop Gorgone And Remove Gorgone Config    push_zmq_gorgone_poller_2
    Sleep    10
    # check what is the normal number of file descriptor for gorgone to take
    ${initial_fd_nb}    Run    ${cmd_count_file_descriptor}
    Log To Console    \n number of file descriptor on before killing poller : ${before_kill_fd_nb} and after : ${initial_fd_nb} \n
    ${max}=    Evaluate    ${initial_fd_nb} + (${before_kill_fd_nb} - ${initial_fd_nb}) * 2
    Log To Console    max is ${max}
    Sleep    20
    ${count_over_limit}=    Set Variable    0
    FOR    ${i}    IN RANGE    60
        ${current_fd_nb}    Run    ${cmd_count_file_descriptor}

        Log To Console    exec\t${i}\t got ${current_fd_nb}, over limit count is ${count_over_limit}
        IF    ${current_fd_nb} > ${max}
            ${count_over_limit}=    Evaluate    ${count_over_limit} + 1
        ELSE
            ${count_over_limit}=    Set Variable    0
        END
        IF    ${count_over_limit} > 10
            Fail    gorgone is using more and more file descriptor after a poller disconnect, starting at ${initial_fd_nb} and after ${i} iteration (2 sec each) to ${current_fd_nb}
        END
        Sleep    2
    END
