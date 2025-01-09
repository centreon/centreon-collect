#
# Copyright 2024 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

!include "FileFunc.nsh"


var cmdline_parameters
var silent_install_cma
var silent_install_plugins

/**
  * @brief write an error message to stdout and exit 1
*/
Function silent_fatal_error
    System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
    System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
    FileWrite $0 "$1$\n"
    SetErrorLevel 1
    Quit
FunctionEnd

/**
  * @brief displays all options in silent mode to stdout and exit 2
*/
Function show_help

    ClearErrors
    ${GetOptions} $cmdline_parameters "--help" $0
    ${IfNot} ${Errors}
        System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
        System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
        FileWrite $0 "usage: centreon-monitoring-agent.exe args$\n"
        FileWrite $0 "This installer works into mode:$\n"
        FileWrite $0 "  - Without argument: interactive windows UI$\n"
        FileWrite $0 "  - Silent mode with the /S flag in first position, before others arguments$\n"
        FileWrite $0 "Silent mode arguments:$\n"
        ${If} $1 != ""
            FileWrite $0 "$1$\n"
        ${EndIf}
        FileWrite $0 "--hostname           The name of the host as defined in the Centreon interface.$\n"
        FileWrite $0 "--endpoint           IP address of DNS name of the poller the agent will connect to.$\n"
        FileWrite $0 "                     In case of Poller-initiated connection mode, it is the interface and port on which the agent will accept connections from the poller. 0.0.0.0 means all interfaces.$\n"
        FileWrite $0 "                     The format is <IP or DNS name>:<port>$\n"
        FileWrite $0 "--reverse            Add this flag for Poller-initiated connection mode.$\n"
        FileWrite $0 "$\n"
        FileWrite $0 "--log_type           event_log or file. In case of logging in a file, log_file param is mandatory $\n"
        FileWrite $0 "--log_level          can be off, critical, error, warning, debug or trace$\n"
        FileWrite $0 "--log_file           log files path.$\n"
        FileWrite $0 "--log_max_file_size  max file in Mo before rotate. $\n"
        FileWrite $0 "--log_max_files      max number of log files before delete. $\n"
        FileWrite $0 "                     For the rotation of logs to be active, it is necessary that both parameters 'Max File Size' and 'Max number of files' are set. The space used by the logs of the agent will not exceed 'Max File Size' * 'Max number of files'. $\n"
        FileWrite $0 "$\n"
        FileWrite $0 "--encryption          Add this flag for encrypt connection with poller.$\n"
        FileWrite $0 "--private_key         Private key file path. Mandatory if encryption and poller-initiated connection are active.$\n"
        FileWrite $0 "--public_cert         Public certificate file path. Mandatory if encryption and poller-initiated connection are active.$\n"
        FileWrite $0 "--ca                  Trusted CA's certificate file path.$\n"
        FileWrite $0 "--ca_name             Expected TLS certificate common name (CN). Don't use it if unsure.$\n"
        SetErrorLevel 2
        Quit
    ${EndIf}
FunctionEnd


/**
  * @brief displays version in silent mode to stdout and exit 2
*/
Function show_version
    ${GetParameters} $cmdline_parameters

    ClearErrors
    ${GetOptions} $cmdline_parameters "--version" $0
    ${IfNot} ${Errors}
        System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
        System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
        FileWrite $0 "Centreon Monitoring Agent installer version:${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}$\n"
        SetErrorLevel 2
        Quit
    ${EndIf}
FunctionEnd


/**
  * @brief checks if user is an admin and output an error message to stdout if not
*/
Function silent_verify_admin
    UserInfo::GetAccountType
    pop $0
    ${If} $0 != "admin" ;Require admin rights
        StrCpy $1 "Administrator rights required!"
        Call silent_fatal_error
    ${EndIf}
FunctionEnd


/**
  * @brief fill registry with cmdline parameters
  * used by installer
*/
Function cmd_line_to_registry
    StrCpy $1 ${FILE_PATH_REGEXP}

    SetRegView 64

    #setup
    ClearErrors
    ${GetOptions} $cmdline_parameters "--hostname" $0
    ${If} ${Errors}
    ${OrIf} $0 == ""
        StrCpy $1 "Empty host name not allowed"
        Call silent_fatal_error
    ${EndIf}
    WriteRegStr HKLM ${CMA_REG_KEY} "host" "$0"
    ${If} ${Errors}
        StrCpy $1 "Failed to write registry key for host"
        Call silent_fatal_error
    ${EndIf}

    ClearErrors
    ${GetOptions} $cmdline_parameters "--endpoint" $0
    ${If} ${Errors}
    ${OrIf} $0 !~ '[a-zA-Z0-9\.\-_]+:[0-9]+'
        StrCpy $1 "The correct format for poller end point or listening interface is <IP or DNS name>:<port>, actual parameter is $0"
        Call silent_fatal_error
    ${EndIf}
    WriteRegStr HKLM ${CMA_REG_KEY} "endpoint" "$0"

    ClearErrors
    ${GetOptions} $cmdline_parameters "--reverse" $0
    ${If} ${Errors}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "reversed_grpc_streaming" 0
        Strcpy $2 0
    ${Else}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "reversed_grpc_streaming" 1
        Strcpy $2 1
    ${EndIf}

    #log
    ClearErrors
    ${GetOptions} $cmdline_parameters "--log_type" $0
    ${IfNot} ${Errors}
    ${AndIf} $0 == "file"
        WriteRegStr HKLM ${CMA_REG_KEY} "log_type"  "File"
        ClearErrors
        ${GetOptions} $cmdline_parameters "--log_file" $0
        ${If} ${Errors}
        ${OrIf} $0 !~ $1
            StrCpy $1 "Bad log file path, actual parameter is $0"
            Call silent_fatal_error
        ${EndIf}
        WriteRegStr HKLM ${CMA_REG_KEY} "log_type"  "file"
        WriteRegStr HKLM ${CMA_REG_KEY} "log_file" $0

        ${GetOptions} $cmdline_parameters "--log_max_file_size" $0
        ${If} ${Errors}
            WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_file_size" 0
        ${Else}
            WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_file_size" $0
        ${EndIf}

        ${GetOptions} $cmdline_parameters "--log_max_files" $0
        ${If} ${Errors}
            WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_files" 0
        ${Else}
            WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_files" $0
        ${EndIf}

    ${Else}
        WriteRegStr HKLM ${CMA_REG_KEY} "log_type"  "event-log"
    ${EndIf}
    ClearErrors
    ${GetOptions} $cmdline_parameters "--log_level" $0
    ${IfNot} ${Errors}
        ${If} $0 == 'off'
        ${OrIf} $0 == 'critical'
        ${OrIf} $0 == 'error'
        ${OrIf} $0 == 'warning'
        ${OrIf} $0 == 'debug'
        ${OrIf} $0 == 'trace'
            ${StrCase} $0 $0 "L"
            WriteRegStr HKLM ${CMA_REG_KEY} "log_level"  $0
        ${Else}
            Strcpy $1 "log_level must be one of off, critical, error, warning, debug or trace"
            Call silent_fatal_error
        ${EndIf}
    ${Else}
        WriteRegStr HKLM ${CMA_REG_KEY} "log_level"  "error"
    ${EndIf}

    #encryption
    ClearErrors
    ${GetOptions} $cmdline_parameters "--encryption" $0
    ${IfNot} ${Errors}
        StrCpy $0 ""
        ${GetOptions} $cmdline_parameters "--private_key" $0
        ${If} ${Errors}
            ${If} $2 == 1
                Strcpy $1 "If encryption and poller-initiated connection are active, the private key is mandatory."
                Call silent_fatal_error
            ${EndIf}
        ${Else}
            ${If} $0 !~ $1
                Strcpy $1 "Bad private key file path."
                Call silent_fatal_error
            ${EndIf}
        ${EndIf}
        WriteRegStr HKLM ${CMA_REG_KEY} "private_key" $0

        StrCpy $0 ""
        ${GetOptions} $cmdline_parameters "--public_cert" $0
        ${If} ${Errors}
            ${If} $2 == 1
                Strcpy $1 "If encryption and poller-initiated connection are active, the certificate is mandatory."
                Call silent_fatal_error
            ${EndIf}
        ${Else}
            ${If} $0 !~ $1
                Strcpy $1 "Bad certificate file path."
                Call silent_fatal_error
            ${EndIf}
        ${EndIf}
        WriteRegStr HKLM ${CMA_REG_KEY} "certificate" $0

        StrCpy $0 ""
        ${GetOptions} $cmdline_parameters "--ca" $0
        ${IfNot} ${Errors}
            ${If} $0 !~ $1
                Strcpy $1 "Bad CA file path."
                Call silent_fatal_error
            ${EndIf}
        ${EndIf}
        WriteRegStr HKLM ${CMA_REG_KEY} "ca_certificate" $0

        StrCpy $0 ""
        ${GetOptions} $cmdline_parameters "--ca_name" $0
        WriteRegStr HKLM ${CMA_REG_KEY} "ca_name" $0
        WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 1
    ${Else}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 0
    ${EndIf}

FunctionEnd

/**
  * @brief fill registry with cmdline parameters
  * used by conf updater/modifier
*/
Function silent_update_conf
    StrCpy $1 ${FILE_PATH_REGEXP}

    SetRegView 64

    #setup
    ClearErrors
    ${GetOptions} $cmdline_parameters "--hostname" $0
    ${IfNot} ${Errors}
        WriteRegStr HKLM ${CMA_REG_KEY} "host" "$0"
    ${EndIf}
    ClearErrors
    ${GetOptions} $cmdline_parameters "--endpoint" $0
    ${IfNot} ${Errors}
        ${If} $0 !~ '[a-zA-Z0-9\.\-_]+:[0-9]+'
            StrCpy $1 "The correct format for poller end point or listening interface is <IP or DNS name>:<port>"
            Call silent_fatal_error
        ${EndIf}
        WriteRegStr HKLM ${CMA_REG_KEY} "endpoint" "$0"
    ${EndIf}
    ClearErrors
    ${GetOptions} $cmdline_parameters "--reverse" $0
    ${IfNot} ${Errors}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "reversed_grpc_streaming" 1
    ${EndIf}
    ClearErrors
    ${GetOptions} $cmdline_parameters "--no_reverse" $0
    ${IfNot} ${Errors}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "reversed_grpc_streaming" 0
    ${EndIf}
    
    #log
    ClearErrors
    ${GetOptions} $cmdline_parameters "--log_type" $0
    ${IfNot} ${Errors}
        ${If} $0 == "file"
            WriteRegStr HKLM ${CMA_REG_KEY} "log_type"  "file"
        ${Else}
            WriteRegStr HKLM ${CMA_REG_KEY} "log_type"  "event-log"
        ${EndIf}
    ${EndIf}
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "log_type"
    ${If} $0 == "file"
        ClearErrors
        ${GetOptions} $cmdline_parameters "--log_file" $0
        ${IfNot} ${Errors}
            ${If} $0 !~ $1
                StrCpy $1 "Bad log file path"
                Call silent_fatal_error
            ${EndIf}
            WriteRegStr HKLM ${CMA_REG_KEY} "log_file" $0
        ${EndIf}
        ClearErrors
        ${GetOptions} $cmdline_parameters "--log_max_file_size" $0
        ${IfNot} ${Errors}
            WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_file_size" $0
        ${EndIf}
        ${GetOptions} $cmdline_parameters "--log_max_files" $0
        ${IfNot} ${Errors}
            WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_files" $0
        ${EndIf}
    ${EndIf}
    ClearErrors
    ${GetOptions} $cmdline_parameters "--log_level" $0
    ${IfNot} ${Errors}
        ${If} $0 == 'off'
        ${OrIf} $0 == 'critical'
        ${OrIf} $0 == 'error'
        ${OrIf} $0 == 'warning'
        ${OrIf} $0 == 'debug'
        ${OrIf} $0 == 'trace'
            ${StrCase} $0 $0 "L"
            WriteRegStr HKLM ${CMA_REG_KEY} "log_level"  $0
        ${Else}
            Strcpy $1 "log_level must be one of off, critical, error, warning, debug or trace"
            Call silent_fatal_error
        ${EndIf}
    ${EndIf}

    #encryption
    ClearErrors
    ${GetOptions} $cmdline_parameters "--encryption" $0
    ${IfNot} ${Errors}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 0
    ${EndIf}
    ReadRegDWORD $0 HKLM ${CMA_REG_KEY} "encryption"
    ${If} $0 > 0
        ${GetOptions} $cmdline_parameters "--private_key" $0
        ${IfNot} ${Errors}
            ${If} $0 !~ $1
                Strcpy $1 "Bad private key file path."
                Call silent_fatal_error
            ${EndIf}
            WriteRegStr HKLM ${CMA_REG_KEY} "private_key" $0
        ${EndIf}
        ${GetOptions} $cmdline_parameters "--public_cert" $0
        ${IfNot} ${Errors}
            ${If} $0 !~ $1
                Strcpy $1 "Bad certificate file path."
                Call silent_fatal_error
            ${EndIf}
            WriteRegStr HKLM ${CMA_REG_KEY} "certificate" $0
        ${EndIf}
        ${GetOptions} $cmdline_parameters "--ca" $0
        ${IfNot} ${Errors}
            ${If} $0 !~ $1
                Strcpy $1 "Bad CA file path."
                Call silent_fatal_error
            ${EndIf}
            WriteRegStr HKLM ${CMA_REG_KEY} "ca_certificate" $0
        ${EndIf}

        ${GetOptions} $cmdline_parameters "--ca_name" $0
        ${IfNot} ${Errors}
            WriteRegStr HKLM ${CMA_REG_KEY} "ca_name" $0
        ${EndIf}

        WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 1
    ${EndIf}

    ClearErrors
    ${GetOptions} $cmdline_parameters "--no_encryption" $0
    ${IfNot} ${Errors}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 0
    ${EndIf}

    #certif and private key are mandatory in reverse mode
    ReadRegDWORD $0 HKLM ${CMA_REG_KEY} "reversed_grpc_streaming"
    ${If} $0 > 0
        ReadRegDWORD $0 HKLM ${CMA_REG_KEY} "encryption"
        ${If} $0 > 0
            ReadRegStr $0 HKLM ${CMA_REG_KEY} "private_key"
            ${If} $0 == ""
                WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 0
                Strcpy $1 "If encryption and poller-initiated connection are active, the private key is mandatory."
                Call silent_fatal_error
            ${EndIf}
            ReadRegStr $0 HKLM ${CMA_REG_KEY} "certificate"
            ${If} $0 == ""
                WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 0
                Strcpy $1 "If encryption and poller-initiated connection are active, the certificate is mandatory."
                Call silent_fatal_error
            ${EndIf}
        ${EndIf}
    ${EndIf}


FunctionEnd

/**
  * @brief checks --install_plugins, --install_embedded_plugins and --install_cma cmdline flags
*/
Function installer_parse_cmd_line
    Push $0

    ClearErrors
    ${GetOptions} $cmdline_parameters "--install_embedded_plugins" $0
    ${IfNot} ${Errors}
        StrCpy $silent_install_plugins 2
    ${EndIf}

    ClearErrors
    ${GetOptions} $cmdline_parameters "--install_plugins" $0
    ${IfNot} ${Errors}
        StrCpy $silent_install_plugins 1
    ${EndIf}

    ClearErrors
    ${GetOptions} $cmdline_parameters "--install_cma" $0
    ${IfNot} ${Errors}
        StrCpy $silent_install_cma 1
    ${EndIf}

    Pop $0
FunctionEnd


/**
  * @brief display help uninstaller
*/
Function un.show_uninstaller_help
    ClearErrors
    ${GetOptions} $cmdline_parameters "--help" $0
    ${IfNot} ${Errors}
        System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
        System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
        FileWrite $0 "usage: uninstaller.exe args$\n"
        FileWrite $0 "Silent mode arguments:$\n"
        FileWrite $0 "--uninstall_cma           uninstall centreon-monitoring-agent$\n"
        FileWrite $0 "--uninstall_plugins       uninstall Centreon plugins$\n"
        SetErrorLevel 2
        Quit
    ${EndIf}
FunctionEnd


/**
  * @brief display uninstaller version
*/
Function un.show_version
    ${GetParameters} $cmdline_parameters

    ClearErrors
    ${GetOptions} $cmdline_parameters "--version" $0
    ${IfNot} ${Errors}
        System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
        System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
        FileWrite $0 "Centreon Monitoring Agent uninstaller version:${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}$\n"
        SetErrorLevel 2
        Quit
    ${EndIf}
FunctionEnd

/**
  * @brief checks if user is an admin and output an error message to stdout if not
*/
Function un.silent_verify_admin
    UserInfo::GetAccountType
    pop $0
    ${If} $0 != "admin" ;Require admin rights
        System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
        System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
        FileWrite $0 "Administrator rights required!$\n"
        SetErrorLevel 1
        Quit
    ${EndIf}
FunctionEnd