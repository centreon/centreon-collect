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

!include "NSISpcre.nsh"

!insertmacro REMatches


/***************************************************************************************
                                     setup dialogbox
***************************************************************************************/

/**
  * @brief initilalize controls with registry contents
*/
Function init_setup_dlg
    Push $0

    SetRegView 64
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "host"
    ${If} $0 == ""
        ReadRegStr $0 HKLM "System\CurrentControlSet\Control\ComputerName\ActiveComputerName" "ComputerName"
    ${EndIf}
    ${NSD_SetText} $hCtl_cma_host_name $0
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "endpoint"
    ${NSD_SetText} $hCtl_cma_endpoint $0
    ReadRegDWORD $0 HKLM ${CMA_REG_KEY} "reversed_grpc_streaming"
    ${If} $0 > 0
        ${NSD_Check} $hCtl_cma_reverse
    ${EndIf}

    Call reverse_onClick

    Pop $0
FunctionEnd

/**
  * @brief validation handler
*/
Function setup_dlg_onNext
    Push $0
    Push $1
    Var /GLOBAL reversed_checked
    ${NSD_GetState} $hCtl_cma_reverse $reversed_checked

    SetRegView 64
    ${NSD_GetText} $hCtl_cma_host_name $0
    ${If} $0 == ""
        MessageBox MB_OK|MB_ICONSTOP "Empty host name not allowed"
        Pop $1
        Pop $0
        Abort
    ${EndIf}

    ${NSD_GetText} $hCtl_cma_endpoint $1
    ${If} $1 !~ "[a-zA-Z0-9\.\-_]+:[0-9]+"
        ${If} $reversed_checked == ${BST_CHECKED}
            MessageBox MB_OK|MB_ICONSTOP "The correct format for the listening interface is <IP or DNS name>:<port>." 
        ${Else}
            MessageBox MB_OK|MB_ICONSTOP "The correct format for the endpoint is <IP or DNS name>:<port>." 
        ${EndIf}
        Pop $1
        Pop $0
        Abort
    ${EndIf}

    WriteRegStr HKLM ${CMA_REG_KEY} "host" "$0"
    WriteRegStr HKLM ${CMA_REG_KEY} "endpoint" "$1"

    ${If} $reversed_checked == ${BST_CHECKED}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "reversed_grpc_streaming" 1
    ${Else}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "reversed_grpc_streaming" 0
	${EndIf}

    Pop $1
    Pop $0
FunctionEnd

/**
  * @brief Poller-initiated connection checkbox onClick handler
*/
Function reverse_onClick
    Push $0
    ${NSD_GetState} $hCtl_cma_reverse $0
    ${If} $0 == ${BST_CHECKED}
        ${NSD_SetText} $hCtl_cma_endpoint_label "Listening interface:"
    ${Else}
        ${NSD_SetText} $hCtl_cma_endpoint_label "Poller endpoint:"
	${EndIf}

    Pop $0
FunctionEnd

/**
  * @brief hostname I image onClick handler
*/
Function hostname_help_onClick
    MessageBox MB_ICONINFORMATION "The name of the host as defined in the Centreon interface."
FunctionEnd

/**
  * @brief endpoint I image onClick handler
*/
Function endpoint_help_onClick
    Push $0
    ${NSD_GetState} $hCtl_cma_reverse $0
    ${If} $0 == ${BST_CHECKED}
        MessageBox MB_ICONINFORMATION "Interface and port on which the agent will accept connections from the poller. 0.0.0.0 means all interfaces."
    ${Else}
        MessageBox MB_ICONINFORMATION "IP address of DNS name of the poller the agent will connect to."
    ${EndIf}
    Pop $0
FunctionEnd

/**
  * @brief Poller-initiated connection checkbox I image onClick handler
*/
Function reverse_help_onClick
    MessageBox MB_ICONINFORMATION "Use when the agent cannot connect to the poller directly: the poller will initiate the connection."
FunctionEnd


/***************************************************************************************
                                     log dialogbox
***************************************************************************************/

/**
  * @brief initilalize controls with registry contents
*/
Function init_log_dlg
    Push $0

    SetRegView 64
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "log_file"
    ${If} $0 != ""
        ${NSD_SetText} $hCtl_log_dlg_log_file_Txt $0
    ${EndIf}
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "log_level"
    ${If} $0 == ""
        StrCpy $0 "error"
    ${EndIf}
    ${NSD_CB_SelectString} $hCtl_log_dlg_log_level $0
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "log_type"
    ${If} $0 == ""
        StrCpy $0 "Event-Log"
    ${EndIf}
    ${NSD_CB_SelectString} $hCtl_log_dlg_log_type $0
    ReadRegDWORD $0 HKLM ${CMA_REG_KEY} "log_max_file_size"
    ${If} $0 > 0
        ${NSD_SetText} $hCtl_log_dlg_max_file_size $0
    ${EndIf}
    ReadRegDWORD $0 HKLM ${CMA_REG_KEY} "log_max_files"
    ${If} $0 > 0
        ${NSD_SetText} $hCtl_log_dlg_max_files $0
    ${EndIf}

    Pop $0
FunctionEnd


/**
  * @brief validation handler
*/
Function log_dlg_onNext
    Push $0

    SetRegView 64
    ${NSD_GetText} $hCtl_log_dlg_log_level $0
    ${If} $0 != ""
        ${StrCase} $0 $0 "L"
        WriteRegStr HKLM ${CMA_REG_KEY} "log_level" "$0"
    ${EndIf}

    ${NSD_GetText} $hCtl_log_dlg_log_type $0
    ${If} $0 == ""
        Pop $0
        Return
    ${EndIf}

    ${If} $0 == "File"
        ${NSD_GetText} $hCtl_log_dlg_log_file_Txt $0
        Push $1
        StrCpy $1 ${FILE_PATH_REGEXP}
        ${If} $0 !~ $1
            MessageBox MB_OK|MB_ICONSTOP "Bad log file path"
            Pop $1
            Pop $0
            Abort
        ${EndIf}
        Pop $1
        WriteRegStr HKLM ${CMA_REG_KEY} "log_type" "file"
        WriteRegStr HKLM ${CMA_REG_KEY} "log_file" $0
        ${NSD_GetText} $hCtl_log_dlg_max_file_size $0
        WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_file_size" $0
        ${NSD_GetText} $hCtl_log_dlg_max_files $0
        WriteRegDWORD HKLM ${CMA_REG_KEY} "log_max_files" $0
    ${Else}
        ${StrCase} $0 $0 "L"
        WriteRegStr HKLM ${CMA_REG_KEY} "log_type" "event-log"
    ${EndIf}

    Pop $0

FunctionEnd

/**
  * @brief when user choose log to file or log to EventLogger, file control group is hidden or shown
*/
Function on_log_type_changed
    ${NSD_GetText} $hCtl_log_dlg_log_type $0

    ${If}  $0 == "File"
        ShowWindow $hCtl_log_dlg_file_group ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_label_max_files ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_max_files ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_max_file_size ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_label_max_file_size ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_label_log_file ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_log_file_Txt ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_log_file_Btn ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_max_files_help ${SW_SHOW}
        ShowWindow $hCtl_log_dlg_max_file_size_help ${SW_SHOW}
    ${Else}
        ShowWindow $hCtl_log_dlg_file_group ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_label_max_files ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_max_files ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_max_file_size ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_label_max_file_size ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_label_log_file ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_log_file_Txt ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_log_file_Btn ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_max_files_help ${SW_HIDE}
        ShowWindow $hCtl_log_dlg_max_file_size_help ${SW_HIDE}
    ${EndIf}

FunctionEnd

/**
  * @brief max file I image onClick handler
*/
Function max_files_help_onClick
    MessageBox MB_ICONINFORMATION "For the rotation of logs to be active, it is necessary that both parameters 'Max File Size' and 'Max number of files' are set. The space used by the logs of the agent will not exceed 'Max File Size' * 'Max number of files'."
FunctionEnd

/**
  * @brief max file size I image onClick handler
*/
Function max_file_size_help_onClick
    MessageBox MB_ICONINFORMATION "For the rotation of logs to be active, it is necessary that both parameters 'Max File Size' and 'Max number of files' are set. The space used by the logs of the agent will not exceed 'Max File Size' * 'Max number of files'."
FunctionEnd



/***************************************************************************************
                                     encryption dialogbox
***************************************************************************************/

/**
  * @brief initilalize controls with registry contents
*/
Function init_encryption_dlg
    Push $0

    SetRegView 64
    ClearErrors
    ReadRegDWORD $0 HKLM ${CMA_REG_KEY} "encryption"
    ${If} ${Errors} 
    ${OrIf} $0 > 0
        ${NSD_Check} $hCtl_encryption_EncryptionCheckBox
    ${EndIf}
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "public_cert"
    ${NSD_SetText} $hCtl_encryption_certificate_file_Txt $0
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "private_key"
    ${NSD_SetText} $hCtl_encryption_private_key_file_Txt $0
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "ca_certificate"
    ${NSD_SetText} $hCtl_encryption_ca_file_Txt $0
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "ca_name"
    ${NSD_SetText} $hCtl_encryption_ca_name $0
    ReadRegStr $0 HKLM ${CMA_REG_KEY} "token"
    ${NSD_SetText} $hCtl_encryption_token $0

    ReadRegDWORD $1 HKLM ${CMA_REG_KEY} "reversed_grpc_streaming"
    ${If} $1 > 0
      ${NSD_SetText} $hCtl_encryption_label_token "Trusted token:"
    ${Else}
      ${NSD_SetText} $hCtl_encryption_label_token "Token:"
    ${EndIf}


    Pop $0
FunctionEnd


/**
  * @brief validation handler
*/
Function encryption_dlg_onNext
    Push $0
    Push $1
    Var /GLOBAL reverse_connection

    ReadRegDWORD $reverse_connection HKLM ${CMA_REG_KEY} "reversed_grpc_streaming"


    StrCpy $0 ${FILE_PATH_REGEXP}

    ${NSD_GetState} $hCtl_encryption_EncryptionCheckBox $1
    ${If} $1 == ${BST_CHECKED}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 1

        ${NSD_GetText} $hCtl_encryption_certificate_file_Txt $1
        ${If} $1 == ""
            ${If} $reverse_connection > 0
                MessageBox MB_OK|MB_ICONSTOP "If encryption and poller-initiated connection are active, the certificate is mandatory."
                Pop $1
                Pop $0
                Abort
            ${EndIf}
        ${Else}
            ${If}  $1 !~ $0
                MessageBox MB_OK|MB_ICONSTOP "Bad certificate file path."
                Pop $1
                Pop $0
                Abort
            ${EndIf}
        ${EndIf}
        Push $2
        ${NSD_GetText} $hCtl_encryption_private_key_file_Txt $2
        ${If} $2 == ""
            ${If} $reverse_connection > 0
                MessageBox MB_OK|MB_ICONSTOP "If encryption and poller-initiated connection are active, the private key is mandatory."
                Pop $2
                Pop $1
                Pop $0
                Abort
            ${EndIf}
        ${Else}
            ${If} $2 !~ $0
                MessageBox MB_OK|MB_ICONSTOP "Bad private key file path."
                Pop $2
                Pop $1
                Pop $0
                Abort
            ${EndIf}
        ${EndIf}
        Push $3
        ${NSD_GetText} $hCtl_encryption_ca_file_Txt $3
        ${If} $3 != "" 
        ${AndIf} $3 !~ $0
            MessageBox MB_OK|MB_ICONSTOP "Bad CA file path."
            Pop $3
            Pop $2
            Pop $1
            Pop $0
            Abort
        ${EndIf}

        Push $4
        ${NSD_GetText} $hCtl_encryption_token $4
        ${If} $4 == ""
            MessageBox MB_OK|MB_ICONSTOP "Token cannot be empty if encryption is enabled."
            Pop $4
            Pop $3
            Pop $2
            Pop $1
            Pop $0
            Abort
        ${EndIf}

        WriteRegStr HKLM ${CMA_REG_KEY} "public_cert" $1
        WriteRegStr HKLM ${CMA_REG_KEY} "private_key" $2
        WriteRegStr HKLM ${CMA_REG_KEY} "ca_certificate" $3
        ${NSD_GetText} $hCtl_encryption_ca_name $1
        WriteRegStr HKLM ${CMA_REG_KEY} "ca_name" $1
        ${If} $reverse_connection > 0
            WriteRegStr HKLM ${CMA_REG_KEY} "trusted_tokens" $4
        ${Else}
            WriteRegStr HKLM ${CMA_REG_KEY} "token" $4
        ${EndIf}
        Pop $4
        Pop $3
        Pop $2
    ${Else}
        WriteRegDWORD HKLM ${CMA_REG_KEY} "encryption" 0
	${EndIf}

    Pop $1
    Pop $0
FunctionEnd

/**
  * @brief when encryption checkbox is checked or not, encryption group is shown or hidden
*/
Function on_encryptioncheckbox_click
    Push $0
    ${NSD_GetState} $hCtl_encryption_EncryptionCheckBox $0
    ${If} $0 == ${BST_CHECKED}
        ShowWindow $hCtl_encryption_EncryptionGroupBox ${SW_SHOW}
        ShowWindow $hCtl_encryption_label_private_key_file ${SW_SHOW}
        ShowWindow $hCtl_encryption_private_key_file_Txt ${SW_SHOW}
        ShowWindow $hCtl_encryption_private_key_file_Btn ${SW_SHOW}
        ShowWindow $hCtl_encryption_label_certificate_file ${SW_SHOW}
        ShowWindow $hCtl_encryption_certificate_file_Txt ${SW_SHOW}
        ShowWindow $hCtl_encryption_certificate_file_Btn ${SW_SHOW}
        ShowWindow $hCtl_encryption_label_ca_file ${SW_SHOW}
        ShowWindow $hCtl_encryption_ca_file_Txt ${SW_SHOW}
        ShowWindow $hCtl_encryption_ca_file_Btn ${SW_SHOW}
        ShowWindow $hCtl_encryption_label_ca_name ${SW_SHOW}
        ShowWindow $hCtl_encryption_ca_name ${SW_SHOW}
        ShowWindow $hCtl_encryption_ca_name_help ${SW_SHOW}
        ShowWindow $hCtl_encryption_ca_file_help ${SW_SHOW}
        ShowWindow $hCtl_encryption_certificate_file_help ${SW_SHOW}
        ShowWindow $hCtl_encryption_private_key_file_help ${SW_SHOW}
        ShowWindow $hCtl_encryption_token ${SW_SHOW}
        ShowWindow $hCtl_encryption_label_token ${SW_SHOW}
        ShowWindow $hCtl_encryption_token_help ${SW_SHOW}
    ${Else}
        ShowWindow $hCtl_encryption_EncryptionGroupBox ${SW_HIDE}
        ShowWindow $hCtl_encryption_label_private_key_file ${SW_HIDE}
        ShowWindow $hCtl_encryption_private_key_file_Txt ${SW_HIDE}
        ShowWindow $hCtl_encryption_private_key_file_Btn ${SW_HIDE}
        ShowWindow $hCtl_encryption_label_certificate_file ${SW_HIDE}
        ShowWindow $hCtl_encryption_certificate_file_Txt ${SW_HIDE}
        ShowWindow $hCtl_encryption_certificate_file_Btn ${SW_HIDE}
        ShowWindow $hCtl_encryption_label_ca_file ${SW_HIDE}
        ShowWindow $hCtl_encryption_ca_file_Txt ${SW_HIDE}
        ShowWindow $hCtl_encryption_ca_file_Btn ${SW_HIDE}
        ShowWindow $hCtl_encryption_label_ca_name ${SW_HIDE}
        ShowWindow $hCtl_encryption_ca_name ${SW_HIDE}
        ShowWindow $hCtl_encryption_ca_name_help ${SW_HIDE}
        ShowWindow $hCtl_encryption_ca_file_help ${SW_HIDE}
        ShowWindow $hCtl_encryption_certificate_file_help ${SW_HIDE}
        ShowWindow $hCtl_encryption_private_key_file_help ${SW_HIDE}
        ShowWindow $hCtl_encryption_token ${SW_HIDE}
        ShowWindow $hCtl_encryption_label_token ${SW_HIDE}
        ShowWindow $hCtl_encryption_token_help ${SW_HIDE}
	${EndIf}
    Pop $0
FunctionEnd


/**
  * @brief private key file I image onClick handler
*/
Function private_key_file_help_onClick
    MessageBox MB_ICONINFORMATION "Private key file path. Mandatory if encryption and poller-initiated connection are active."
FunctionEnd

/**
  * @brief certificate file I image onClick handler
*/
Function certificate_file_help_onClick
    MessageBox MB_ICONINFORMATION "Public certificate file path. Mandatory if encryption and poller-initiated connection are active."
FunctionEnd

/**
  * @brief ca file I image onClick handler
*/
Function ca_file_help_onClick
    MessageBox MB_ICONINFORMATION "Trusted CA's certificate file."
FunctionEnd

/**
  * @brief ca name I image onClick handler
*/
Function ca_name_help_onClick
    MessageBox MB_ICONINFORMATION "Expected TLS certificate common name (CN) - leave blank if unsure."
FunctionEnd

/**
  * @brief ca name I image onClick handler
*/
Function token_help_onClick
    MessageBox MB_ICONINFORMATION "Expected JWT(Json Web Token)"
FunctionEnd