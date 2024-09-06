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

Set-PSDebug -Trace 2

Write-Host "Work in" $pwd.ToString()

$current_dir = (pwd).Path
$wsl_path =  "/mnt/" + $current_dir.SubString(0,1).ToLower() + "/" + $current_dir.SubString(3).replace('\','/')

mkdir reports

reg import agent/conf/centagent.reg

Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name log_type -Value file
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name host -Value host_1

#in wsl1, no VM, so IP address are identical in host and wsl
#windows can connect to linux on localhost but linux must use host ip
$my_host_name = $env:COMPUTERNAME
$my_ip = (Get-NetIpAddress -AddressFamily IPv4 | Where-Object IPAddress -ne "127.0.0.1" | SELECT IPAddress -First 1).IPAddress

openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout server_grpc.key -out server_grpc.crt -subj "/CN=${my_host_name}"

Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name endpoint -Value ${my_host_name}:4317
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name reversed_grpc_streaming -Value 0
$agent_log_path = $current_dir + "\reports\centagent.log"
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name log_file -Value $agent_log_path

#Start agent
Start-Process -FilePath build_windows\agent\Release\centagent.exe -RedirectStandardOutput reports\centagent_stdout.log -RedirectStandardError reports\centagent_stderr.log 

Write-Host ($agent_process | Format-Table | Out-String)

Start-Sleep -Seconds 1

#encrypted version
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name ca_certificate -Value ${$current_dir}/server_grpc.crt
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name endpoint -Value ${my_host_name}:4318
$agent_log_path = $current_dir + "\reports\encrypted_centagent.log"
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name log_file -Value $agent_log_path

Start-Process -FilePath build_windows\agent\Release\centagent.exe -RedirectStandardOutput reports\encrypted_centagent_stdout.log -RedirectStandardError reports\encrypted_centagent_stderr.log


Start-Sleep -Seconds 1

#Start reverse agent
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name ca_certificate -Value ""
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name endpoint -Value 0.0.0.0:4320
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name reversed_grpc_streaming -Value 1
$agent_log_path = $current_dir + "\reports\reverse_centagent.log"
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name log_file -Value $agent_log_path

Start-Process -FilePath build_windows\agent\Release\centagent.exe -RedirectStandardOutput reports\reversed_centagent_stdout.log -RedirectStandardError reports\reversed_centagent_stderr.log

Start-Sleep -Seconds 1

#reversed and encrypted
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name private_key -Value ${$current_dir}/server_grpc.key
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name public_cert -Value ${$current_dir}/server_grpc.crt
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name endpoint -Value 0.0.0.0:4321
$agent_log_path = $current_dir + "\reports\encrypted_reverse_centagent.log"
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name log_file -Value $agent_log_path

Start-Process -FilePath build_windows\agent\Release\centagent.exe -RedirectStandardOutput reports\encrypted_reversed_centagent_stdout.log -RedirectStandardError reports\encrypted_reversed_centagent_stderr.log

wsl cd $wsl_path `&`& .github/scripts/wsl-collect-test-robot.sh broker-engine/cma.robot $my_host_name $my_ip

if (Test-Path -Path 'reports\windows-cma-failed' -PathType Container) {
    exit 1
}

Stop-Process -Name centagent.exe

