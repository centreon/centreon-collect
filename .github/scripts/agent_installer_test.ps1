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


function test_args_to_registry {
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
    param (
        [string] $exe_path,
        [string[]] $exe_args,
        $expected_registry_values
    )

    Write-Host "arguments: $exe_args"

    $process_info= Start-Process -PassThru  $exe_path $exe_args
    $process_info.WaitForExit()
    if ($process_info.ExitCode -ne 0) {
        Write-Host "fail to execute $exe_path with arguments $exe_args"
        Write-Host "exit status = " $process_info.ExitCode
        exit 1
    }
    
    #let time to windows to flush registry
    Start-Sleep  -Seconds 2

    foreach ($value_name in $expected_registry_values.Keys) {
        $expected_value = $($expected_registry_values[$value_name])
        $real_value = (Get-ItemProperty -Path HKLM:\Software\Centreon\CentreonMonitoringAgent -Name $value_name).$value_name
        if ($expected_value -ne $real_value) {
            Write-Host "unexpected value for $value_name, expected: $expected_value, read: $real_value"
            exit 1
        }
    }
}

Write-Host "############################  all install uninstall   ############################"

$args = '/S','--install_cma', '--install_plugins', '--hostname', "my_host_name_1", "--endpoint","127.0.0.1:4317"
$expected = @{ 'endpoint'='127.0.0.1:4317';'host'='my_host_name_1';'log_type'='EventLog'; 'log_level' = 'error'; 'encryption' = 0;'reversed_grpc_streaming'= 0 }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $args $expected

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

$process_info= Start-Process -PassThru  "C:\Program Files\Centreon\CentreonMonitoringAgent\uninstall.exe" "/S", "--uninstall_cma","--uninstall_plugins"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 0) {
    Write-Host "bad uninstaller exit code"
    exit 1
}

Start-Sleep -Seconds 5

Get-Process | Select-Object -Property ProcessName | Select-String centagent

$info = Get-Process | Select-Object -Property ProcessName | Select-String centagent
#$info = Get-Process centagent 2>$null
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


Write-Host "############################  installer test  ############################"

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--help"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 2) {
    Write-Host "bad --help exit code"
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--version"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 2) {
    Write-Host "bad --version exit code"
    exit 1
}

#missing mandatory parameters
$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no parameter exit code " $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no endpoint exit code " $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","turlututu"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad wrong endpoint exit code " $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","127.0.0.1:4317","--log_type","file"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad no log file path " $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","127.0.0.1:4317","--log_type","file","--log_file","C:"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad log file path " $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","127.0.0.1:4317","--log_level","dsfsfd"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "bad log level " $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","127.0.0.1:4317","--reverse","--log_type","file","--log_file","C:\Users\Public\cma.log","--encryption"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and no private_key " $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","127.0.0.1:4317","--reverse","--log_type","file","--log_file","C:\Users\Public\cma.log","--encryption","--private_key","C:"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and bad private_key path" $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","127.0.0.1:4317","--reverse","--log_type","file","--log_file","C:\Users\Public\cma.log","--encryption","--private_key","C:\Users\Public\private_key.key"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and no certificate" $process_info.ExitCode
    exit 1
}

$process_info= Start-Process -PassThru  "agent/installer/centreon-monitoring-agent.exe" "/S", "--install_cma","--hostname","toto","--endpoint","127.0.0.1:4317","--reverse","--log_type","file","--log_file","C:\Users\Public\cma.log","--encryption","--private_key","C:\Users\Public\private_key.key", "--public_cert", "C:"
Wait-Process -Id $process_info.Id
if ($process_info.ExitCode -ne 1) {
    Write-Host "reverse mode, encryption and bad certificate path" $process_info.ExitCode
    exit 1
}


$args = '/S','--install_cma','--hostname', "my_host_name_1", "--endpoint","127.0.0.1:4317"
$expected = @{ 'endpoint'='127.0.0.1:4317';'host'='my_host_name_1';'log_type'='EventLog'; 'log_level' = 'error'; 'encryption' = 0;'reversed_grpc_streaming'= 0 }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $args $expected

$args = '/S','--install_cma','--hostname', "my_host_name_2", "--endpoint","127.0.0.2:4317", "--log_type", "file",  "--log_file", "C:\Users\Public\cma.log", "--log_level", "trace", "--log_max_file_size", "15", "--log_max_files", "10"
$expected = @{ 'endpoint'='127.0.0.2:4317';'host'='my_host_name_2';'log_type'='File'; 'log_level' = 'trace'; 'log_file'='C:\Users\Public\cma.log'; 'encryption' = 0;'reversed_grpc_streaming'= 0; 'log_max_file_size' = 15; 'log_max_files' = 10; }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $args $expected

$args = '/S','--install_cma','--hostname', "my_host_name_2", "--endpoint","127.0.0.3:4317", "--log_type", "file",  "--log_file", "C:\Users\Public\cma.log", "--log_level", "trace", "--encryption"
$expected = @{ 'endpoint'='127.0.0.3:4317';'host'='my_host_name_2';'log_type'='File'; 'log_level' = 'trace'; 'log_file'='C:\Users\Public\cma.log'; 'encryption' = 1;'reversed_grpc_streaming'= 0 }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $args $expected

$args = '/S','--install_cma','--hostname', "my_host_name_2", "--endpoint","127.0.0.4:4317", "--log_type", "file",  "--log_file", "C:\Users\Public\cma.log", "--log_level", "trace", "--encryption", "--private_key", "C:\Users crypto\private.key", "--public_cert", "D:\tutu\titi.crt", "--ca", "C:\Users\Public\ca.crt", "--ca_name", "tls_ca_name"
$expected = @{ 'endpoint'='127.0.0.4:4317';'host'='my_host_name_2';'log_type'='File'; 'log_level' = 'trace'; 'log_file'='C:\Users\Public\cma.log'; 'encryption' = 1;'reversed_grpc_streaming'= 0; 'certificate'='D:\tutu\titi.crt'; 'private_key'='C:\Users crypto\private.key'; 'ca_certificate' = 'C:\Users\Public\ca.crt'; 'ca_name' = 'tls_ca_name'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $args $expected

$args = '/S','--install_cma','--hostname', "my_host_name_2", "--endpoint","127.0.0.5:4317", "--log_type", "file",  "--log_file", "C:\Users\Public\cma_rev.log", "--log_level", "trace", "--encryption", "--reverse", "--private_key", "C:\Users crypto\private_rev.key", "--public_cert", "D:\tutu\titi_rev.crt", "--ca", "C:\Users\Public\ca_rev.crt", "--ca_name", "tls_ca_name_rev"
$expected = @{ 'endpoint'='127.0.0.5:4317';'host'='my_host_name_2';'log_type'='File'; 'log_level' = 'trace'; 'log_file'='C:\Users\Public\cma_rev.log'; 'encryption' = 1;'reversed_grpc_streaming'= 1; 'certificate'='D:\tutu\titi_rev.crt'; 'private_key'='C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent.exe" $args $expected


Write-Host "############################  modifier test   ############################"

$args = '/S','--hostname', "my_host_name_10", "--endpoint","127.0.0.10:4317", "--no_reverse"
$expected = @{ 'endpoint'='127.0.0.10:4317';'host'='my_host_name_10';'log_type'='File'; 'log_level' = 'trace'; 'log_file'='C:\Users\Public\cma_rev.log'; 'encryption' = 1;'reversed_grpc_streaming'= 0; 'certificate'='D:\tutu\titi_rev.crt'; 'private_key'='C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $args $expected

$args = '/S',"--log_type", "file",  "--log_file", "C:\Users\Public\cma_rev2.log", "--log_level", "debug", "--log_max_file_size", "50", "--log_max_files", "20"
$expected = @{ 'endpoint'='127.0.0.10:4317';'host'='my_host_name_10';'log_type'='File'; 'log_level' = 'debug'; 'log_file'='C:\Users\Public\cma_rev2.log'; 'encryption' = 1;'reversed_grpc_streaming'= 0; 'certificate'='D:\tutu\titi_rev.crt'; 'log_max_file_size' = 50; 'log_max_files' = 20;'private_key'='C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $args $expected

$args = '/S',"--log_type", "EventLog",  "--log_level", "error"
$expected = @{ 'endpoint'='127.0.0.10:4317';'host'='my_host_name_10';'log_type'='event-log'; 'log_level' = 'error';   'encryption' = 1;'reversed_grpc_streaming'= 0; 'certificate'='D:\tutu\titi_rev.crt'; 'private_key'='C:\Users crypto\private_rev.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $args $expected

$args = '/S',"--private_key", "C:\Users crypto\private_rev2.key", "--public_cert", "D:\tutu\titi_rev2.crt"
$expected = @{ 'endpoint'='127.0.0.10:4317';'host'='my_host_name_10';'log_type'='event-log'; 'log_level' = 'error';   'encryption' = 1;'reversed_grpc_streaming'= 0; 'certificate'='D:\tutu\titi_rev2.crt'; 'private_key'='C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev.crt'; 'ca_name' = 'tls_ca_name_rev'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $args $expected

$args = '/S',"--ca", "C:\Users\Public\ca_rev2.crt", "--ca_name", "tls_ca_name_rev2"
$expected = @{ 'endpoint'='127.0.0.10:4317';'host'='my_host_name_10';'log_type'='event-log'; 'log_level' = 'error';   'encryption' = 1;'reversed_grpc_streaming'= 0; 'certificate'='D:\tutu\titi_rev2.crt'; 'private_key'='C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev2.crt'; 'ca_name' = 'tls_ca_name_rev2'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $args $expected

$args = '/S',"--no_encryption"
$expected = @{ 'endpoint'='127.0.0.10:4317';'host'='my_host_name_10';'log_type'='event-log'; 'log_level' = 'error';   'encryption' = 0;'reversed_grpc_streaming'= 0; 'certificate'='D:\tutu\titi_rev2.crt'; 'private_key'='C:\Users crypto\private_rev2.key'; 'ca_certificate' = 'C:\Users\Public\ca_rev2.crt'; 'ca_name' = 'tls_ca_name_rev2'  }
test_args_to_registry "agent/installer/centreon-monitoring-agent-modify.exe" $args $expected



Write-Host "############################  end test   ############################"

exit 0
