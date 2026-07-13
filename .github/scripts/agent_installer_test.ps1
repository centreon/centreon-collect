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

# This script test CMA installer in silent mode

#Set-PSDebug -Trace 2

$installer_exe  = 'agent\installer\centreon-monitoring-agent.exe'
$modifier_exe  = 'agent\installer\centreon-monitoring-agent-modify.exe'
# build the path to the installer and modifier executables
$installer_exepath = Join-Path -Path (Get-Location) -ChildPath $installer_exe
$modifier_exepath = Join-Path -Path (Get-Location) -ChildPath $modifier_exe


function f_start_process([string]$sProcess, [string]$sArgs, [ref]$pOutPut) {
    <#
    .SYNOPSIS
    Starts a new process with the specified executable and arguments.

    .DESCRIPTION
    The `f_start_process` function initiates a new process using the provided executable path and arguments. 

    .PARAMETER sProcess
    The path to the executable file to start.

    .PARAMETER sArgs
    The arguments to pass to the executable.

    .PARAMETER pOutPut
    variable where we will store stdout and stderr
    #>    
    $oProcessInfo = New-Object System.Diagnostics.ProcessStartInfo
    $oProcessInfo.FileName = $sProcess
    $oProcessInfo.RedirectStandardError = $true
    $oProcessInfo.RedirectStandardOutput = $true
    $oProcessInfo.UseShellExecute = $false
    $oProcessInfo.Arguments = $sArgs
    $oProcess = New-Object System.Diagnostics.Process
    $oProcess.StartInfo = $oProcessInfo
    $oProcess.Start() | Out-Null
    $oProcess.WaitForExit() | Out-Null
    $sSTDOUT = $oProcess.StandardOutput.ReadToEnd()
    $sSTDERR = $oProcess.StandardError.ReadToEnd()
    $pOutPut.Value = "Commandline: $sProcess $sArgs`r`n"
    $pOutPut.Value += "STDOUT: " + $sSTDOUT + "`r`n"
    $pOutPut.Value += "STDERR: " + $sSTDERR + "`r`n"
    return $oProcess.ExitCode
}

function test_args_to_registry ([string] $exe_path, [string[]] $exe_args, $expected_registry_values, [string] $agent_instance) {
    <#    
.SYNOPSIS
    start a program and check values in registry

.PARAMETER exe_path
    path of the installer to execute

.PARAMETER exe_args
    installer arguments

.PARAMETER expected_registry_values
    hash_table as @{'host'='host_1';'endpoint'='127.0.0.1'}

.PARAMETER agent_instance
    registry subkey name (AGENTINSTANCE). Mandatory.
#>
    if (-not $agent_instance) {
        Write-Host "agent_instance parameter missing (forgot to pass 4th argument?)"
        exit 1
    }

    Write-Host "execute $exe_path arguments: $exe_args"

    $process_output = @{}
    $exit_code = f_start_process $exe_path $exe_args ([ref]$process_output)
    Write-Host $process_output

    if ($exit_code -ne 0) {
        Write-Host "fail to execute $exe_path with arguments $exe_args"
        Write-Host "exit status = " $process_info.ExitCode
        exit 1
    }
    
    for (($i = 0); $i -lt 10; $i++) {
        Start-Sleep -Seconds 1
        try {
            Write-Host "check registry key for: $agent_instance"
            Get-ItemProperty -Path HKLM:\Software\Centreon\${agent_instance}
            break
        }
        catch { 
            continue
        }
    }

    for (($i = 0); $i -lt 10; $i++) {
        Start-Sleep -Seconds 1
        $read_success = 1
        foreach ($value_name in $expected_registry_values.Keys) {
            $expected_value = $($expected_registry_values[$value_name])
            try {
                $real_value = (Get-ItemProperty -Path HKLM:\Software\Centreon\${agent_instance} -Name $value_name).$value_name
                if ($expected_value -ne $real_value) {
                    Write-Host "unexpected value for $value_name, expected: $expected_value, read: $real_value"
                    $read_success = 0
                    break
                }
            }
            catch { 
                $read_success = 0
                break
            }
        }
        if ($read_success -eq 1) {
            break
        }
    }

    if ($read_success -ne 1) {
        Write-Host "fail to read expected registry values in 10s installer output: $process_output"
        exit 1
    }
}

function test_all_silent_install_uninstall([string]$plugins_flag) {
    <#
    .SYNOPSIS
    test all silent install uninstall

    .DESCRIPTION
    test all silent install uninstall

    .PARAMETER plugins_flag
    Can be '/PLUGINSRC=embedded' or '/PLUGINSRC=auto'

    #>

    Write-Host "############################  all install uninstall with flag: $plugins_flag  ############################"

    $exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS="agent,plugins"', $plugins_flag, '/HOST=my_host_name_1', '/ENDPOINT=127.0.0.1:4317','/token=my_secure_token'
    $expected = @{ 'endpoint' = '127.0.0.1:4317'; 'host' = 'my_host_name_1'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0;'token'='my_secure_token' }
    test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent"

    if (!(Get-ItemProperty -Path HKLM:\Software\Centreon\CentreonMonitoringAgent)) {
        Write-Host "no registry entry created"
        exit 1
    }

    Get-Process | Select-Object -Property ProcessName | Select-String centagent

    $info = Get-Process | Select-Object -Property ProcessName | Select-String centagent

    #$info = Get-Process centagent 2>$null
    if (!$info) {
        Write-Host "centagent.exe not started"
        exit 1
    }

    if (![System.Io.File]::Exists("C:\Program Files\Centreon\Plugins\centreon_plugins.exe")) {
        Write-Host "centreon_plugins.exe not installed"
        exit 1
    }

    $process_info = Start-Process -PassThru  "C:\Program Files\Centreon\unins000.exe" "/VERYSILENT /FULL"
    Wait-Process -Id $process_info.Id
    if ($process_info.ExitCode -ne 0) {
        Write-Host "bad uninstaller exit code"
        exit 1
    }


    for (($i = 0); $i -lt 10; $i++) {
        Start-Sleep -Seconds 1
        $info = Get-Process | Select-Object -Property ProcessName | Select-String centagent
        if (! $info) {
            break
        }
    }

    if ($info) {
        Write-Host "centagent.exe running"
        exit 1
    }

    if ([System.Io.File]::Exists("C:\Program Files\Centreon\Plugins\centreon_plugins.exe")) {
        Write-Host "centreon_plugins.exe not removed"
        exit 1
    }

    #the only mean I have found to test key erasure under CI
    #Test-Path doesn't work
    $key_found = $true
    try {
        Get-ChildItem -Path HKLM:\Software\Centreon\CentreonMonitoringAgent -ErrorAction Stop
    }
    catch { 
        $key_found = $false
    }

    if ($key_found) {
        Write-Host "registry entry not removed"
        exit 1
    }

    if ($info) {
        Write-Host "centagent.exe running"
        exit 1
    }

    if ([System.Io.File]::Exists("C:\Program Files\Centreon\Plugins\centreon_plugins.exe")) {
        Write-Host "centreon_plugins.exe not removed"
        exit 1
    }

    #the only mean I have found to test key erasure under CI
    #Test-Path doesn't work
    $key_found = $true
    try {
        Get-ChildItem -Path HKLM:\Software\Centreon\CentreonMonitoringAgent -ErrorAction Stop
    }
    catch { 
        $key_found = $false
    }

    if ($key_found) {
        Write-Host "registry entry not removed"
        exit 1
    }

    Start-Sleep -Seconds 10
}

test_all_silent_install_uninstall("/PLUGINSRC=auto")
test_all_silent_install_uninstall("/PLUGINSRC=embedded")

Write-Host "############################  installer test  ############################"

# $process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "--help"
# Wait-Process -Id $process_info.Id
# if ($process_info.ExitCode -ne 2) {
#     Write-Host "bad --help exit code"
#     exit 1
# }

# $process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "--version"
# Wait-Process -Id $process_info.Id
# if ($process_info.ExitCode -ne 2) {
#     Write-Host "bad --version exit code"
#     exit 1
# }

#missing mandatory parameters
$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/TYPE=custom /COMPONENTS=agent"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no parameter exit code " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/TYPE=custom /COMPONENTS=agent", "/HOST=toto"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no endpoint exit code " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/TYPE=custom /COMPONENTS=agent", "/HOST=toto", "/ENDPOINT=turlututu"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad wrong endpoint exit code " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/type=custom /components=agent", "/HOST=toto", "/ENDPOINT=127.0.0.1:4317", "/LOGTYPE=File"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no log file path " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/type=custom /components=agent", "/HOST=toto", "/ENDPOINT=127.0.0.1:4317", "/LOGTYPE=File", '/LOGFILE="C:"'
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad log file path " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/type=custom /components=agent", "/HOST=toto", "/ENDPOINT=127.0.0.1:4317", '/LOGLEVEL=dsfsfd'
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad log level " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/type=custom /components=agent", "/HOST=toto", "/ENDPOINT=127.0.0.1:4317", "/REVERSE=true", "/LOGTYPE=File", '/LOGFILE="C:\Users\Public\cma.log"', "/ENCRYPTION=full"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and no private_key " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/type=custom /components=agent", "/HOST=toto", "/ENDPOINT=127.0.0.1:4317", "/REVERSE=true", "/LOGTYPE=File", '/LOGFILE="C:\Users\Public\cma.log"', "/ENCRYPTION=full", '/KEY="C:"'
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and bad private_key path" $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/type=custom /components=agent", "/HOST=toto", "/ENDPOINT=127.0.0.1:4317", "/REVERSE=true", "/LOGTYPE=File", '/LOGFILE="C:\Users\Public\cma.log"', "/ENCRYPTION=full", '/KEY="C:\Users\Public\private_key.key"'
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and no certificate" $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  $installer_exepath "/VERYSILENT", "/type=custom /components=agent", "/HOST=toto", "/ENDPOINT=127.0.0.1:4317", "/REVERSE=true", "/LOGTYPE=File", '/LOGFILE="C:\Users\Public\cma.log"', "/ENCRYPTION=full", '/KEY="C:\Users\Public\private_key.key"', '/CERT="C:"'
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and bad certificate path" $process_info.ExitCode
    exit 1
}

# host_template default value
$exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_default', '/ENDPOINT=127.0.0.10:4317', '/TOKEN=my_secure_token_default'
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_default'; 'host_template' = 'OS-Windows-Centreon-Monitoring-Agent-custom'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0; 'token' = 'my_secure_token_default' }
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent"

# host_template custom value
$exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_custom', '/HOSTTEMPLATE=Custom-Template-01', '/ENDPOINT=127.0.0.11:4317', '/TOKEN=my_secure_token_custom'
$expected = @{ 'endpoint' = '127.0.0.11:4317'; 'host' = 'my_host_name_custom'; 'host_template' = 'Custom-Template-01'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0; 'token' = 'my_secure_token_custom' }
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent1"

$exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_1', '/ENDPOINT=127.0.0.1:4317','/token=my_secure_token'
$expected = @{ 'endpoint' = '127.0.0.1:4317'; 'host' = 'my_host_name_1'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0 ;'token'='my_secure_token'}
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent2"

$exe_args = '/VERYSILENT', '/AGENTINSTANCE=toto', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_2', '/ENDPOINT=127.0.0.2:4317', '/LOGTYPE=File', '/LOGFILE="C:\Users\Public\cma.log"', '/LOGLEVEL=trace', '/MAXFILESIZE=15', '/MAXNUMBER=10','/token=my_secure_token'
$expected = @{ 'endpoint' = '127.0.0.2:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma.log'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0; 'log_max_file_size' = 15; 'log_max_files' = 10; 'token' = 'my_secure_token' }
test_args_to_registry $installer_exepath $exe_args $expected "toto"

$exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_2', '/ENDPOINT=127.0.0.3:4317', '/LOGTYPE=File', '/LOGFILE="C:\Users\Public\cma.log"', '/LOGLEVEL=trace', '/ENCRYPTION=no','/token=my_secure_token'
$expected = @{ 'endpoint' = '127.0.0.3:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma.log'; 'encryption' = 'no'; 'reversed_grpc_streaming' = 0 ;'token' = 'my_secure_token' }
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent3"

$exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_2', '/ENDPOINT=127.0.0.4:4317', '/LOGTYPE=File', '/LOGFILE="C:\Users\Public\cma.log"', '/LOGLEVEL=trace', '/ENCRYPTION=insecure', '/KEY="C:\Users crypto\private.key"', '/CERT="D:\tutu\titi.crt"', '/CA="C:\Users\Public\ca.crt"', '/COMMONNAME=tls_ca_name', '/TOKEN=my_secure_token1'
$expected = @{ 'endpoint' = '127.0.0.4:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma.log'; 'encryption' = 'insecure'; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi.crt'; 'private_key' = 'C:\Users crypto\private.key'; 'ca_certificate' = 'C:\Users\Public\ca.crt'; 'ca_name' = 'tls_ca_name';'token' = 'my_secure_token1' }
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent4"

$exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_2', '/ENDPOINT=127.0.0.5:4317', '/LOGTYPE=File', '/LOGFILE="C:\Users\Public\cma_rev.log"', '/LOGLEVEL=trace', '/ENCRYPTION=full', '/REVERSE=true', '/KEY="C:\Users crypto\private_rev.key"', '/CERT="D:\tutu\titi_rev.crt"', '/CA="C:\Users\Public\ca_rev.crt"', '/COMMONNAME=tls_ca_name_rev','/TOKEN=my_secure_token1'
$expected = @{ 'endpoint' = '127.0.0.5:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma_rev.log'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 1; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev';'token' = 'my_secure_token1' }
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent5"

$exe_args = '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_3', '/ENDPOINT=127.0.0.5:4317', '/LOGTYPE=File', '/LOGFILE="C:\Users\Public\cma_rev.log"', '/LOGLEVEL=trace', '/ENCRYPTION=full', '/REVERSE=true', '/KEY="C:\Users crypto\private_rev.key"', '/CERT="D:\tutu\titi_rev.crt"', '/CA="C:\Users\Public\ca_rev.crt"', '/COMMONNAME=tls_ca_name_rev', '/TOKEN=my_secure_token1'
$expected = @{ 'endpoint' = '127.0.0.5:4317'; 'host' = 'my_host_name_3'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma_rev.log'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 1; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev'; 'token' = 'my_secure_token1' }
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent6"

# minimal silent install (required args only)
$exe_args = '/VERYSILENT','/HOST=min_host','/TOKEN=my_secure_token1', '/ENDPOINT=127.0.0.30:4317'
$expected = @{ 'endpoint' = '127.0.0.30:4317'; 'host' = 'min_host'; 'host_template' = 'OS-Windows-Centreon-Monitoring-Agent-custom'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0 }
test_args_to_registry $installer_exepath $exe_args $expected "CentreonMonitoringAgent7"


Write-Host "############################  modifier test   ############################"

$modifier_instance = "CentreonMonitoringAgent"

# host_template default value (modifier)
$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=modifier_host_default', '/ENDPOINT=127.0.0.20:4317', '/TOKEN=modifier_token_default'
$expected = @{ 'endpoint' = '127.0.0.20:4317'; 'host' = 'modifier_host_default'; 'host_template' = 'OS-Windows-Centreon-Monitoring-Agent-custom'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0; 'token' = 'modifier_token_default' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

# host_template custom value (modifier)
$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=modifier_host_custom', '/HOSTTEMPLATE=Modifier-Template-01', '/ENDPOINT=127.0.0.21:4317', '/TOKEN=modifier_token_custom'
$expected = @{ 'endpoint' = '127.0.0.21:4317'; 'host' = 'modifier_host_custom'; 'host_template' = 'Modifier-Template-01'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0; 'token' = 'modifier_token_custom' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_10', '/ENDPOINT=127.0.0.10:4317', '/REVERSE=false', '/TOKEN=my_secure_token2', '/LOGTYPE=File', '/LOGLEVEL=trace', '/LOGFILE="C:\Users\Public\cma_rev.log"', '/ENCRYPTION=insecure', '/CERT="D:\tutu\titi_rev.crt"', '/KEY="C:\Users crypto\private_rev.key"', '/CA="C:\Users\Public\ca_rev.crt"', '/COMMONNAME=tls_ca_name_rev'
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma_rev.log'; 'encryption' = 'insecure'; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev'; 'token' = 'my_secure_token2' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_10', '/ENDPOINT=127.0.0.10:4317', '/LOGTYPE=File', '/LOGLEVEL=debug', '/LOGFILE="C:\Users\Public\cma_rev2.log"', '/MAXFILESIZE=50', '/MAXNUMBER=20', '/ENCRYPTION=insecure', '/CERT="D:\tutu\titi_rev.crt"', '/KEY="C:\Users crypto\private_rev.key"', '/CA="C:\Users\Public\ca_rev.crt"', '/COMMONNAME=tls_ca_name_rev'
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'File'; 'log_level' = 'debug'; 'log_file' = 'C:\Users\Public\cma_rev2.log'; 'encryption' = 'insecure'; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'log_max_file_size' = 50; 'log_max_files' = 20; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_10', '/ENDPOINT=127.0.0.10:4317', '/LOGTYPE=event-log', '/LOGLEVEL=error', '/ENCRYPTION=insecure', '/CERT="D:\tutu\titi_rev.crt"', '/KEY="C:\Users crypto\private_rev.key"', '/CA="C:\Users\Public\ca_rev.crt"', '/COMMONNAME=tls_ca_name_rev', '/TOKEN=my_secure_token'
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'insecure'; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev';'token' = 'my_secure_token' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_10', '/ENDPOINT=127.0.0.10:4317', '/LOGTYPE=event-log', '/LOGLEVEL=error', '/ENCRYPTION=full', '/CERT="D:\tutu\titi_rev2.crt"', '/KEY="C:\Users crypto\private_rev2.key"', '/CA="C:\Users\Public\ca_rev.crt"', '/COMMONNAME=tls_ca_name_rev'
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev2.crt'; 'private_key' = 'C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_10', '/ENDPOINT=127.0.0.10:4317', '/LOGTYPE=event-log', '/LOGLEVEL=error', '/ENCRYPTION=full', '/CERT="D:\tutu\titi_rev2.crt"', '/KEY="C:\Users crypto\private_rev2.key"', '/CA="C:\Users\Public\ca_rev2.crt"', '/COMMONNAME=tls_ca_name_rev2'
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'full'; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev2.crt'; 'private_key' = 'C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev2.crt'; 'ca_name' = 'tls_ca_name_rev2' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

$exe_args = '/AGENTINSTANCE=CentreonMonitoringAgent', '/VERYSILENT', '/TYPE=custom /COMPONENTS=agent', '/HOST=my_host_name_10', '/ENDPOINT=127.0.0.10:4317', '/LOGTYPE=event-log', '/LOGLEVEL=error', '/ENCRYPTION=no', '/CERT="D:\tutu\titi_rev2.crt"', '/KEY="C:\Users crypto\private_rev2.key"', '/CA="C:\Users\Public\ca_rev2.crt"', '/COMMONNAME=tls_ca_name_rev2'
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 'no'; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev2.crt'; 'private_key' = 'C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev2.crt'; 'ca_name' = 'tls_ca_name_rev2' }
test_args_to_registry $modifier_exepath $exe_args $expected $modifier_instance

Write-Host "############################  uninstall test   ############################"

$process_info = Start-Process -PassThru  "C:\Program Files\Centreon\unins000.exe" "/VERYSILENT /AGENTINSTANCE=CentreonMonitoringAgent2,CentreonMonitoringAgent3"
Wait-Process -Id $process_info.Id

#the only mean I have found to test key erasure under CI
#Test-Path doesn't work
$key_found = $true
try {
    Get-ChildItem -Path HKLM:\Software\Centreon\CentreonMonitoringAgent2 -ErrorAction Stop
}
catch { 
    $key_found = $false
}

if ($key_found) {
    Write-Host "registry entry not removed"
    exit 1
}

$key_found = $true
try {
    Get-ChildItem -Path HKLM:\Software\Centreon\CentreonMonitoringAgent3 -ErrorAction Stop
}
catch { 
    $key_found = $false
}

if ($key_found) {
    Write-Host "registry entry not removed"
    exit 1
}

# uninstall the last installed instance
Write-Host "uninstall last installed instance"
$process_info = Start-Process -PassThru  "C:\Program Files\Centreon\unins000.exe" "/VERYSILENT /FULL"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 0) {
    Write-Host "bad uninstaller exit code"
    exit 1
}


Write-Host "############################  end test   ############################"

exit 0
