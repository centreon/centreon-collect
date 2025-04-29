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

XPStyle on

Unicode false

!define APPNAME "CentreonMonitoringAgent"
!define COMPANYNAME "Centreon"
!define DESCRIPTION "The Centreon Monitoring Agent (CMA) collects metrics and computes statuses on the servers it monitors, and sends them to Centreon."

!define SERVICE_NAME ${APPNAME}

!define CMA_REG_KEY "SOFTWARE\${COMPANYNAME}\${APPNAME}"

#Match to windows file path C:\tutu yoyo1234 titi\fgdfgdg.rt
!define FILE_PATH_REGEXP '^[a-zA-Z]:([\\|\/](([\w\.]+\s+)*[\w\.]+)+)+$$'
modifier la regex pour autoriser les - et _
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

!define NSCLIENT_URL "https://api.github.com/repos/centreon/centreon-nsclient-build/releases/latest"

!define NSISCONF_3 ";" ; NSIS 2 tries to parse some preprocessor instructions inside "!if 0" blocks!
!addincludedir "nsis_pcre"
!define /redef NSISCONF_3 ""
${NSISCONF_3}!addplugindir /x86-ansi "nsis_pcre"
${NSISCONF_3}!addplugindir /x86-ansi "inetc/Plugins/x86-ansi"
${NSISCONF_3}!addplugindir /x86-ansi "ns_json/Plugins/x86-ansi"
!undef NSISCONF_3


!include "LogicLib.nsh"
!include "nsDialogs.nsh"
!include "mui.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"
!include "FileFunc.nsh"

!include "nsis_service_lib.nsi"

${Using:StrFunc} StrCase

!include "version.nsi"
!include "resources\setup_dlg.nsdinc"
!include "resources\encryption_dlg.nsdinc"
!include "resources\log_dlg.nsdinc"
#let it after dialog boxes
!include "dlg_helper.nsi"

!include "silent.nsi"

OutFile "centreon-monitoring-agent.exe"
Name "Centreon Monitoring Agent ${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"
Icon "resources/logo_centreon.ico"
LicenseData "resources/license.txt"
RequestExecutionLevel admin
;AllowRootDirInstall true

VIProductVersion "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}.0"
VIFileVersion "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}.0"
VIAddVersionKey "FileVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"
VIAddVersionKey "LegalCopyright" "2024 Centreon"
VIAddVersionKey "FileDescription" "Centreon Monitoring Agent Installer"
VIAddVersionKey "ProductName" "Centreon Monitoring Agent"
VIAddVersionKey "CompanyName" "Centreon"
VIAddVersionKey "ProductVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}.0"


InstallDir "$PROGRAMFILES64\${COMPANYNAME}\${APPNAME}"
!define PLUGINS_DIR "$PROGRAMFILES64\${COMPANYNAME}\Plugins"
!define PLUGINS_FULL_PATH "${PLUGINS_DIR}\centreon_plugins.exe"

!define HELPURL "https://www.centreon.com/"

Var plugins_url
Var plugins_download_failure


!macro verify_user_is_admin
UserInfo::GetAccountType
pop $0
${If} $0 != "admin" ;Require admin rights
        messageBox mb_iconstop "Administrator rights required!"
        setErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
        quit
${EndIf}
!macroend
 
 

/**
  * @brief pages
*/
page license
Page components
Page custom setup_cma_show setup_dlg_onNext
Page custom setup_log_show log_dlg_onNext ": logging"
Page custom setup_cma_encryption_show encryption_dlg_onNext ": encryption"
Page instfiles

/**
 * @brief first it uses github API to get information of centreon-nsclient-build last releases.
 * Then it gets a json response where one asset is the asset of centreon plugins
 *
*/
Function get_plugins_url
    #because of several bugs, we have to store github response in a file and then parse it
    Var /GLOBAL json_content_path
    Var /GLOBAL nb_assets 
    GetTempFileName $json_content_path
    #get release information
    ClearErrors
    inetc::get /header "Accept: application/vnd.github+json" ${NSCLIENT_URL} $json_content_path /End
    ${If} ${Errors}
        MessageBox MB_YESNO "Failed to get latest Centreon plugins from ${NSCLIENT_URL}.$\nDo you want to install local Centreon plugins (version ${PLUGINS_VERSION})?" /SD IDYES IDYES continue_with_embedded_plugins IDNO continue_without_plugins
    ${EndIf}
    Pop $0
    ${If} $0 != "OK"
        MessageBox MB_YESNO "Failed to get latest Centreon plugins from ${NSCLIENT_URL}.$\nDo you want to install local Centreon plugins (version ${PLUGINS_VERSION})?" /SD IDYES IDYES continue_with_embedded_plugins IDNO continue_without_plugins
    ${EndIf}

    #parse json response
    nsJSON::Set /file $json_content_path
    ${If} ${Errors}
        MessageBox MB_YESNO "Bad json received from  ${NSCLIENT_URL}.$\nDo you want to install local Centreon plugins (version ${PLUGINS_VERSION})?" /SD IDYES IDYES continue_with_embedded_plugins IDNO continue_without_plugins
    ${EndIf}

    nsJSON::Get /count `assets` /end
    Pop $nb_assets

    ${ForEach} $0 0 $nb_assets + 1
        ClearErrors
        nsJSON::Get "assets" /index $0 "name"
        ${IfNot} ${Errors}
            Pop $1
            ${If} $1 == "centreon_plugins.exe"
                nsJSON::Get "assets" /index $0 "browser_download_url"
                Pop $plugins_url
                Return
            ${EndIf}
        ${EndIf}        
    ${Next}

    MessageBox MB_YESNO "No Plugins found at ${NSCLIENT_URL} $\nDo you want to install local Centreon plugins (version ${PLUGINS_VERSION})?" /SD IDYES IDYES continue_with_embedded_plugins IDNO continue_without_plugins
    continue_without_plugins:
        StrCpy $plugins_download_failure 2
        Return
    continue_with_embedded_plugins:
        StrCpy $plugins_download_failure 1
        Return

FunctionEnd

/**
  * @brief this section download plugings from the asset of the last centreon-nsclient-build release
*/
Section "Plugins" PluginsInstSection
    CreateDirectory ${PLUGINS_DIR}
    ${IfNot} ${Silent}
        Call get_plugins_url
        ${If} $plugins_download_failure == 1
            DetailPrint "Install centreon plugins version ${PLUGINS_VERSION}"
            File /oname=${PLUGINS_FULL_PATH} "centreon_plugins.exe"
        ${ElseIf} $plugins_download_failure == 2
            DetailPrint 'centreon plugins not installed'
        ${Else}
            DetailPrint "download plugins from $plugins_url"
            ClearErrors
            inetc::get /caption "plugins"  /banner "Downloading plugins..." "$plugins_url" "${PLUGINS_DIR}/centreon_plugins.exe"
            ${If} ${Errors}
                MessageBox MB_YESNO "Failed to download latest Centreon plugins.$\nDo you want to install local Centreon plugins (version ${PLUGINS_VERSION})?" /SD IDYES IDYES ui_continue_with_embedded_plugins IDNO ui_continue_without_plugins
                ui_continue_with_embedded_plugins:
                    File /oname=${PLUGINS_FULL_PATH} "centreon_plugins.exe"
                    DetailPrint "Local Centreon plugins (version ${PLUGINS_VERSION}) installed"
                ui_continue_without_plugins:
                    DetailPrint 'Centreon plugins have not been installed'
            ${EndIf}
        ${EndIf}

    ${Else}
        ${If} $silent_install_plugins == 2
            File /oname=${PLUGINS_FULL_PATH} "centreon_plugins.exe"
            System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
            System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
            FileWrite $0 "Local Centreon plugins (version ${PLUGINS_VERSION}) installed$\n"
        ${Else}
            Call get_plugins_url
            ${If} $plugins_download_failure > 0
                File /oname=${PLUGINS_FULL_PATH} "centreon_plugins.exe"
                System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
                System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
                FileWrite $0 "Failed to download latest Centreon plugins => local Centreon plugins (version ${PLUGINS_VERSION}) installed$\n"
            ${Else}
                ClearErrors
                inetc::get /caption "plugins"  /banner "Downloading plugins..." "$plugins_url" "${PLUGINS_DIR}/centreon_plugins.exe"
                ${If} ${Errors}
                    File /oname=${PLUGINS_FULL_PATH} "centreon_plugins.exe"
                    System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
                    System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
                    FileWrite $0 "Failed to download latest Centreon plugins => local Centreon plugins (version ${PLUGINS_VERSION}) installed$\n"
                ${Else}
                    System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
                    System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
                    FileWrite $0 "Centreon plugins installed$\n"
                ${EndIf}
            ${EndIf}
        ${EndIf}
    ${EndIf}
SectionEnd


/**
  * @brief this section configure and install centreon monitoring agent
*/
Section "Centreon Monitoring Agent"  CMAInstSection
    SetRegView 64
    #event logger
    WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\EventLog\Application\${SERVICE_NAME}" "TypesSupported" 7
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Services\EventLog\Application\${SERVICE_NAME}" "EventMessageFile" "%systemroot%\System32\mscoree.dll"
    setOutPath $INSTDIR

    !insertmacro SERVICE "stop" "${SERVICE_NAME}" ""

    #wait for service stop
    StrCpy $0 ""
    ${Do}
        Push  "running"
        Push ${SERVICE_NAME}
        Push ""
        Call Service
        Pop $0
        ${If} $0 != "true"
            ${Break}
        ${EndIf}
        Sleep 500
    ${Loop}
    # even if service is stopped, process can be stopping so we wait a little more
    Sleep 500

    file ${CENTAGENT_PATH}
    file "centreon-monitoring-agent-modify.exe"
    writeUninstaller "$INSTDIR\uninstall.exe"

    #create and start service
    !insertmacro SERVICE "create" "${SERVICE_NAME}" \
        "path=$INSTDIR\centagent.exe;autostart=1;display=Centreon Monitoring Agent;\
        starttype=${SERVICE_AUTO_START};servicetype=${SERVICE_WIN32_OWN_PROCESS}"
    !insertmacro SERVICE "start" "${SERVICE_NAME}" ""
    
    #uninstall information
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "Centreon Monitoring Agent"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "${UNINSTALL_KEY}" "ModifyPath" "$\"$INSTDIR\centreon-monitoring-agent-modify.exe$\""
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "EstimatedSize" "$0"

    ${If} ${Silent}
        System::Call 'kernel32::AttachConsole(i -1)i.r0' ;attach to parent console
        System::Call 'kernel32::GetStdHandle(i -11)i.r0' ;console attached -- get stdout
        FileWrite $0 "Centreon monitoring agent installed and started$\n"
    ${EndIf}
SectionEnd


/**
  * @brief function called on install
*/
function .onInit
	setShellVarContext all

    ${If} ${Silent}
        SetErrorLevel 0
        ${GetParameters} $cmdline_parameters
        Strcpy $1 "--install_cma        Set this flag if you want to install centreon monitoring agent$\n\
--install_plugins    Set this flag if you want to download and install latest version of centreon plugins$\n\
--install_embedded_plugins Set this flag if you want to install the plugins embedded in the installer$\n"
        Call show_help
        Call show_version
        Call silent_verify_admin

        Call installer_parse_cmd_line

        ${If} $silent_install_cma == 1
            Call cmd_line_to_registry
            SectionSetFlags ${CMAInstSection} ${SF_SELECTED}
        ${Else}
            SectionSetFlags ${CMAInstSection} 0
        ${EndIf}

        ${If} $silent_install_plugins > 0
            SectionSetFlags ${PluginsInstSection} ${SF_SELECTED}
        ${Else}
            SectionSetFlags ${PluginsInstSection} 0
        ${EndIf}

    ${Else}
    	!insertmacro verify_user_is_admin
    ${EndIf}

functionEnd

/**
  * @brief show cma setup dialogbox ig user has choosen to install cma
*/
Function setup_cma_show
    ${If} ${SectionIsSelected} ${CMAInstSection}
        Call fnc_cma_Show
    ${EndIf}    
FunctionEnd

/**
  * @brief show cma log dialogbox if user has choosen to install cma
*/
Function setup_log_show
    ${If} ${SectionIsSelected} ${CMAInstSection}
        Call fnc_log_dlg_Show
    ${EndIf}    
FunctionEnd

/**
  * @brief show cma encryption dialogbox if user has choosen to install cma
*/
Function setup_cma_encryption_show
    ${If} ${SectionIsSelected} ${CMAInstSection}
        Call fnc_encryption_Show
    ${EndIf}    
FunctionEnd


/**
  * @brief uninstall section
*/
Section "uninstall" UninstallSection
    SetRegView 64
    # the only way to delete a service without reboot
    ExecWait 'net stop ${SERVICE_NAME}'
    ExecWait 'sc delete ${SERVICE_NAME}'

    delete $INSTDIR\centagent.exe
    delete $INSTDIR\centreon-monitoring-agent-modify.exe
 
    #cma
    DeleteRegKey HKLM "${CMA_REG_KEY}"
    DeleteRegKey /ifempty HKLM "Software\Centreon"

    #event logger
    DeleteRegKey HKLM "SYSTEM\CurrentControlSet\Services\EventLog\Application\${SERVICE_NAME}"

	# Always delete uninstaller as the last action
	delete $INSTDIR\uninstall.exe

	# Try to remove the install directory - this will only happen if it is empty
	rmDir $INSTDIR
    rmDir "$PROGRAMFILES64\${COMPANYNAME}"

    DeleteRegKey HKLM "${UNINSTALL_KEY}"
SectionEnd

/**
  * @brief called on uninstall
*/
function un.onInit
	SetShellVarContext all

    ${If} ${Silent}
        SetErrorLevel 0
        Call un.show_uninstaller_help
        Call un.show_version
        Call un.silent_verify_admin

        ClearErrors
        ${GetOptions} $cmdline_parameters "--uninstall_plugins" $0
        ${IfNot} ${Errors}
            rmDir /r ${PLUGINS_DIR}
        ${EndIf}

        ClearErrors
        ${GetOptions} $cmdline_parameters "--uninstall_cma" $0
        ${IfNot} ${Errors}
            SectionSetFlags ${UninstallSection} ${SF_SELECTED}
        ${Else}
            SectionSetFlags ${UninstallSection} 0
        ${EndIf}

    ${Else}
        !insertmacro verify_user_is_admin
    
        MessageBox MB_YESNO "Do you want to remove the Centreon plugins for the agents?" IDNO no_plugins_remove
        rmDir /r ${PLUGINS_DIR}
        no_plugins_remove:

        MessageBox MB_YESNO "Do you want to remove the Centreon Monitoring Agent?" IDYES no_cma_remove
            Abort
        no_cma_remove:

    ${EndIf}
functionEnd
