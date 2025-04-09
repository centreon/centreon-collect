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

function test_args_to_registry ([string] $exe_path, [string[]] $exe_args, $expected_registry_values) {
    <#    
.SYNOPSIS
    start a program and check values in registry

.PARAMETER exe_path
    path of the installer to execute

.PARAMETER exe_args
    installer arguments

.PARAMETER expected_registry_values
    hash_table as @{'host'='host_1';'endpoint'='127.0.0.1'}
#>

    Write-Host "execute $exe_path arguments: $exe_args"

    $process_output = @{}
    $exit_code = f_start_process $exe_path $exe_args ([ref]$process_output)

    if ($exit_code -ne 0) {
        Write-Host "fail to execute $exe_path with arguments $exe_args"
        Write-Host "exit status = " $process_info.ExitCode
        exit 1
    }
    
    for (($i = 0); $i -lt 10; $i++) {
        Start-Sleep -Seconds 1
        try {
            Get-ItemProperty -Path HKLM:\Software\Centreon\CentreonMonitoringAgent
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
                $real_value = (Get-ItemProperty -Path HKLM:\Software\Centreon\CentreonMonitoringAgent -Name $value_name).$value_name
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
    Can be --install_plugins or --install_embedded_plugins

    #>



    Write-Host "############################  all install uninstall with flag: $plugins_flag  ############################"

    $exe_args = '/S', '--install_cma', $plugins_flag, '--hostname', "my_host_name_1", "--endpoint", "127.0.0.1:4317"
    $expected = @{ 'endpoint' = '127.0.0.1:4317'; 'host' = 'my_host_name_1'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 0; 'reversed_grpc_streaming' = 0 }
    test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $exe_args $expected

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

    $process_info = Start-Process -PassThru  "C:\Program Files\Centreon\CentreonMonitoringAgent\uninstall.exe" "/S", "--uninstall_cma", "--uninstall_plugins"
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

    Write-Host "The followind command will output errors, don't take it into account"
    #the only mean I have found to test key erasure under CI
    #Test-Path doesn't work
    $key_found = true
    try {
        Get-ChildItem -Path HKLM:\Software\Centreon\CentreonMonitoringAgent
    }
    catch { 
        $key_found = false
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

    Write-Host "The followind command will output errors, don't take it into account"
    #the only mean I have found to test key erasure under CI
    #Test-Path doesn't work
    $key_found = true
    try {
        Get-ChildItem -Path HKLM:\Software\Centreon\CentreonMonitoringAgent
    }
    catch { 
        $key_found = false
    }

    if ($key_found) {
        Write-Host "registry entry not removed"
        exit 1
    }

    Start-Sleep -Seconds 10
}

test_all_silent_install_uninstall("--install_plugins")
test_all_silent_install_uninstall("--install_embedded_plugins")

Write-Host "############################  installer test  ############################"

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--help"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 2) {
    Write-Host "bad --help exit code"
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--version"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 2) {
    Write-Host "bad --version exit code"
    exit 1
}

#missing mandatory parameters
$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no parameter exit code " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no endpoint exit code " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "turlututu"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad wrong endpoint exit code " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "127.0.0.1:4317", "--log_type", "file"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no log file path " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "127.0.0.1:4317", "--log_type", "file", "--log_file", "C:"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad log file path " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "127.0.0.1:4317", "--log_level", "dsfsfd"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad log level " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "127.0.0.1:4317", "--reverse", "--log_type", "file", "--log_file", "C:\Users\Public\cma.log", "--encryption"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and no private_key " $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "127.0.0.1:4317", "--reverse", "--log_type", "file", "--log_file", "C:\Users\Public\cma.log", "--encryption", "--private_key", "C:"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and bad private_key path" $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "127.0.0.1:4317", "--reverse", "--log_type", "file", "--log_file", "C:\Users\Public\cma.log", "--encryption", "--private_key", "C:\Users\Public\private_key.key"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and no certificate" $process_info.ExitCode
    exit 1
}

$process_info = Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma", "--hostname", "toto", "--endpoint", "127.0.0.1:4317", "--reverse", "--log_type", "file", "--log_file", "C:\Users\Public\cma.log", "--encryption", "--private_key", "C:\Users\Public\private_key.key", "--public_cert", "C:"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and bad certificate path" $process_info.ExitCode
    exit 1
}


$exe_args = '/S', '--install_cma', '--hostname', "my_host_name_1", "--endpoint", "127.0.0.1:4317"
$expected = @{ 'endpoint' = '127.0.0.1:4317'; 'host' = 'my_host_name_1'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 0; 'reversed_grpc_streaming' = 0 }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $exe_args $expected

$exe_args = '/S', '--install_cma', '--hostname', "my_host_name_2", "--endpoint", "127.0.0.2:4317", "--log_type", "file", "--log_file", "C:\Users\Public\cma.log", "--log_level", "trace", "--log_max_file_size", "15", "--log_max_files", "10"
$expected = @{ 'endpoint' = '127.0.0.2:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma.log'; 'encryption' = 0; 'reversed_grpc_streaming' = 0; 'log_max_file_size' = 15; 'log_max_files' = 10; }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $exe_args $expected

$exe_args = '/S', '--install_cma', '--hostname', "my_host_name_2", "--endpoint", "127.0.0.3:4317", "--log_type", "file", "--log_file", "C:\Users\Public\cma.log", "--log_level", "trace", "--encryption"
$expected = @{ 'endpoint' = '127.0.0.3:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma.log'; 'encryption' = 1; 'reversed_grpc_streaming' = 0 }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $exe_args $expected

$exe_args = '/S', '--install_cma', '--hostname', "my_host_name_2", "--endpoint", "127.0.0.4:4317", "--log_type", "file", "--log_file", "C:\Users\Public\cma.log", "--log_level", "trace", "--encryption", "--private_key", "C:\Users crypto\private.key", "--public_cert", "D:\tutu\titi.crt", "--ca", "C:\Users\Public\ca.crt", "--ca_name", "tls_ca_name"
$expected = @{ 'endpoint' = '127.0.0.4:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma.log'; 'encryption' = 1; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi.crt'; 'private_key' = 'C:\Users crypto\private.key'; 'ca_certificate' = 'C:\Users\Public\ca.crt'; 'ca_name' = 'tls_ca_name' }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $exe_args $expected

$exe_args = '/S', '--install_cma', '--hostname', "my_host_name_2", "--endpoint", "127.0.0.5:4317", "--log_type", "file", "--log_file", "C:\Users\Public\cma_rev.log", "--log_level", "trace", "--encryption", "--reverse", "--private_key", "C:\Users crypto\private_rev.key", "--public_cert", "D:\tutu\titi_rev.crt", "--ca", "C:\Users\Public\ca_rev.crt", "--ca_name", "tls_ca_name_rev"
$expected = @{ 'endpoint' = '127.0.0.5:4317'; 'host' = 'my_host_name_2'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma_rev.log'; 'encryption' = 1; 'reversed_grpc_streaming' = 1; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev' }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $exe_args $expected

$exe_args = '/S', '--install_cma', '--hostname', "my_host_name_3", "--endpoint", "127.0.0.5:4317", "--log_type", "file", "--log_file", "C:\Users\Public\cma_rev.log", "--log_level", "trace", "--encryption","--reverse", "--private_key", "C:\Users crypto\private_rev.key", "--public_cert", "D:\tutu\titi_rev.crt", "--ca", "C:\Users\Public\ca_rev.crt", "--ca_name", "tls_ca_name_rev",'--token', 'my_secure_token'
$expected = @{ 'endpoint' = '127.0.0.5:4317'; 'host' = 'my_host_name_3'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma_rev.log'; 'encryption' = 1; 'reversed_grpc_streaming' = 1; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev';'token' = 'my_secure_token' }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $exe_args $expected

Write-Host "############################  modifier test   ############################"

$exe_args = '/S', '--hostname', "my_host_name_10", "--endpoint", "127.0.0.10:4317", "--no_reverse","--token", "my_secure_token2"
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'File'; 'log_level' = 'trace'; 'log_file' = 'C:\Users\Public\cma_rev.log'; 'encryption' = 1; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev';'token' = 'my_secure_token2' }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $exe_args $expected

$exe_args = '/S', "--log_type", "file", "--log_file", "C:\Users\Public\cma_rev2.log", "--log_level", "debug", "--log_max_file_size", "50", "--log_max_files", "20"
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'File'; 'log_level' = 'debug'; 'log_file' = 'C:\Users\Public\cma_rev2.log'; 'encryption' = 1; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'log_max_file_size' = 50; 'log_max_files' = 20; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev' }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $exe_args $expected

$exe_args = '/S', "--log_type", "event-log", "--log_level", "error"
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 1; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev.crt'; 'private_key' = 'C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev' }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $exe_args $expected

$exe_args = '/S', "--private_key", "C:\Users crypto\private_rev2.key", "--public_cert", "D:\tutu\titi_rev2.crt"
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 1; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev2.crt'; 'private_key' = 'C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev' }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $exe_args $expected

$exe_args = '/S', "--ca", "C:\Users\Public\ca_rev2.crt", "--ca_name", "tls_ca_name_rev2"
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 1; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev2.crt'; 'private_key' = 'C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev2.crt'; 'ca_name' = 'tls_ca_name_rev2' }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $exe_args $expected

$exe_args = '/S', "--no_encryption"
$expected = @{ 'endpoint' = '127.0.0.10:4317'; 'host' = 'my_host_name_10'; 'log_type' = 'event-log'; 'log_level' = 'error'; 'encryption' = 0; 'reversed_grpc_streaming' = 0; 'public_cert' = 'D:\tutu\titi_rev2.crt'; 'private_key' = 'C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev2.crt'; 'ca_name' = 'tls_ca_name_rev2' }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $exe_args $expected



Write-Host "############################  end test   ############################"

exit 0
