*** Settings ***
Documentation       Start and stop gorgone

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        120s

*** Test Cases ***
Start and stop gorgone
    @{configfile}    Create List    ${CURDIR}${/}config.yaml
    FOR    ${i}    IN RANGE    5
        ${gorgone_name}=    Set Variable    gorgone_start_stop${i}

        Setup Gorgone Config    ${configfile}    gorgone_name=${gorgone_name}
        Log To Console    Starting Gorgone...
        Start Gorgone    debug    ${gorgone_name}
        Sleep    5s
        Log To Console    Stopping Gorgone...
        Stop Gorgone And Remove Gorgone Config    gorgone_start_stop${i}
        sleep    2s
    END