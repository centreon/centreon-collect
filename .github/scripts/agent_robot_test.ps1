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

# This script test windows CMA
# We first start four instances of centreon agent (reverse or not, encryption or not)
# Then, we install collect in a wsl and start robot test on it.
# Used ports are:
#   - 4317 no reversed, no encryption 
#   - 4318 no reversed, encryption 
#   - 4320 reversed, no encryption 
#   - 4321 reversed, encryption
# All files are shared between wsl and windows, we translate it with $wsl_path
# By this share, we use certificates (server.*) on both world
# In order to communicate bteween two worlds, we use hostname and IP of the host
# That's why we rewrite /etc/hosts on wsl side
# agent logs are saved in reports and wsl fail tests are saved in it also in case of failure

param($robot="cma.robot")

Write-Host "Work in" $pwd.ToString()

$current_dir = (pwd).Path
$wsl_path = "/mnt/" + $current_dir.SubString(0, 1).ToLower() + "/" + $current_dir.SubString(3).replace('\', '/')

mkdir reports

reg import agent/conf/centagent.reg

#windows can connect to linux on localhost but linux must use host ip
$my_host_name = $env:COMPUTERNAME

$pwsh_path = (get-command pwsh.exe).Path

echo "host_name:" $my_host_name

#open reverse ports
New-NetFirewallRule -DisplayName "Allow Port 4320" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 4320
New-NetFirewallRule -DisplayName "Allow Port 4321" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 4321

# generate certificate used by wsl and windows
openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout server_grpc.key -out server_grpc.crt -subj "/CN=localhost"
openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout reverse_server_grpc.key -out reverse_server_grpc.crt -subj "/CN=${my_host_name}"

# create custom check file
Set-Content -Path "${current_dir}\reports\custom_check.txt" -Value @(
    '[custom_checks]'
    'check_echo=C:\Windows\System32\cmd.exe /C echo $ARG2$ $ARG1$ from custom check'
    'check_ping=C:\Windows\System32\cmd.exe /C ping 127.0.0.1'
)



$agent_log_path = $current_dir + "\reports\centagent.log"

#The first agent (agent initiated connection, no encryption is started by installer, others will be started manually)
$installer_exe = 'agent\installer\centreon-monitoring-agent.exe'
$installer_exepath = Join-Path -Path (Get-Location) -ChildPath $installer_exe
Write-Host "install agent only  (agent initiated connection, no encryption)"
$installer_args = '/VERYSILENT', '/TYPE=custom', '/COMPONENTS="agent"','/AGENTINSTANCE=CentreonMonitoringAgent1', '/HOST=host_1', '/ENDPOINT=localhost:4317', '/LOGTYPE=File', "/LOGFILE=$agent_log_path", '/LOGLEVEL=trace','/TOKEN=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJjZW50cmVvbjY2MjQxIiwiaWF0IjoxNzQ0MDk3MDgxLCJleHAiOjkyMjMzNzIwMzV9.QkrT77i211-CvXoXqaBxRMzxajzA3-DK-DGVrbvJWA8', "/CUSTOMCHECKFILE=${current_dir}\reports\custom_check.txt"

# Better: get the process, wait
Start-Process -Wait -FilePath $installer_exepath -ArgumentList $installer_args

Start-Sleep -Seconds 5

#encrypted version
$agent_log_path = $current_dir + "\reports\encrypted_centagent.log"

$installer_exe = 'agent\installer\centreon-monitoring-agent.exe'
$installer_exepath = Join-Path -Path (Get-Location) -ChildPath $installer_exe
Write-Host "install agent only  (agent initiated connection, encryption)"
$installer_args = '/VERYSILENT', '/TYPE=custom', '/COMPONENTS="agent"','/AGENTINSTANCE=CentreonMonitoringAgent2', '/HOST=host_1', '/ENDPOINT=localhost:4318', '/LOGTYPE=File', "/LOGFILE=$agent_log_path", '/LOGLEVEL=trace','/ENCRYPTION=full',"/CA=${current_dir}\server_grpc.crt",'/TOKEN=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJjZW50cmVvbjY2MjQxIiwiaWF0IjoxNzQ0MDk3MDgxLCJleHAiOjkyMjMzNzIwMzV9.QkrT77i211-CvXoXqaBxRMzxajzA3-DK-DGVrbvJWA8'
Start-Process -Wait -FilePath $installer_exepath -ArgumentList $installer_args

Start-Sleep -Seconds 5

$agent_log_path = $current_dir + "\reports\centagent_unknown_host.log"

#The third agent (agent initiated connection, no encryption is started by installer, unknown host from engine, others will be started manually)
$installer_exe = 'agent\installer\centreon-monitoring-agent.exe'
$installer_exepath = Join-Path -Path (Get-Location) -ChildPath $installer_exe
Write-Host "install agent only  (agent initiated connection, no encryption)"
$installer_args = '/VERYSILENT', '/TYPE=custom', '/COMPONENTS="agent"','/AGENTINSTANCE=CentreonMonitoringAgent5', '/HOST=We_dont_know_me', '/ENDPOINT=localhost:4319', '/LOGTYPE=File', "/LOGFILE=$agent_log_path", '/LOGLEVEL=trace','/TOKEN=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJjZW50cmVvbjY2MjQxIiwiaWF0IjoxNzQ0MDk3MDgxLCJleHAiOjkyMjMzNzIwMzV9.QkrT77i211-CvXoXqaBxRMzxajzA3-DK-DGVrbvJWA8', "/CUSTOMCHECKFILE=${current_dir}\reports\custom_check.txt"
Start-Process -Wait -FilePath $installer_exepath -ArgumentList $installer_args

Start-Sleep -Seconds 5


#Start reverse agent
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name ca_certificate -Value ""
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name encryption -Value no
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name endpoint -Value 0.0.0.0:4320
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name reversed_grpc_streaming -Value 1
$agent_log_path = $current_dir + "\reports\reverse_centagent.log"
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name log_file -Value $agent_log_path

Start-Sleep -Seconds 2
Write-Host "start manually agent (poller initiated connection, no encryption)"
Start-Process -FilePath build_windows\agent\Release\centagent.exe -ArgumentList "--standalone --service-name CentreonMonitoringAgent1" -RedirectStandardOutput reports\reversed_centagent_stdout.log -RedirectStandardError reports\reversed_centagent_stderr.log

Start-Sleep -Seconds 5

#reversed and encrypted
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name private_key -Value ${current_dir}/reverse_server_grpc.key
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name public_cert -Value ${current_dir}/reverse_server_grpc.crt
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name encryption -Value full
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name endpoint -Value 0.0.0.0:4321
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name token -Value eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJjZW50cmVvbjY2MjQxIiwiaWF0IjoxNzQ0MDk3MDgxLCJleHAiOjkyMjMzNzIwMzV9.QkrT77i211-CvXoXqaBxRMzxajzA3-DK-DGVrbvJWA8
$agent_log_path = $current_dir + "\reports\encrypted_reverse_centagent.log"
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent1  -Name log_file -Value $agent_log_path

Start-Sleep -Seconds 2
Write-Host "start manually agent (poller initiated connection, encryption)"
Start-Process -FilePath build_windows\agent\Release\centagent.exe -ArgumentList "--standalone --service-name CentreonMonitoringAgent1" -RedirectStandardOutput reports\encrypted_reversed_centagent_stdout.log -RedirectStandardError reports\encrypted_reversed_centagent_stderr.log


$uptime = (Get-WmiObject -Class Win32_OperatingSystem).LastBootUpTime #dtmf format
$d_uptime = [Management.ManagementDateTimeConverter]::ToDateTime($uptime)  #datetime format
$ts_uptime = ([DateTimeOffset]$d_uptime).ToUnixTimeSeconds() #timestamp format

$systeminfo_data = systeminfo /FO CSV | ConvertFrom-Csv
$snapshot = @{
    'total'        = $systeminfo_data.'Total Physical Memory'
    'free'         = $systeminfo_data.'Available Physical Memory'
    'virtual_max'  = $systeminfo_data.'Virtual Memory: Max Size'
    'virtual_free' = $systeminfo_data.'Virtual Memory: Available'
}

$serv_list = Get-Service

$serv_stat = @{
    'services.running.count' = ($serv_list  | Where-Object { $_.Status -eq "Running" } | measure).Count
    'services.stopped.count' = ($serv_list  | Where-Object { $_.Status -eq "stopped" } | measure).Count
}

$test_param = @{
    'host'        = $my_host_name
    'wsl_path'    = $wsl_path
    'pwsh_path'   = $pwsh_path
    'drive'       = @()
    'current_dir' = $current_dir.replace('\', '/')
    'uptime'      = $ts_uptime
    'mem_info'    = $snapshot
    'serv_stat'   = $serv_stat
}

Get-PSDrive -PSProvider FileSystem | Select Name, Used, Free | ForEach-Object -Process { $test_param.drive += $_ }

# create 3 task sched
$taskScriptsPath = "$env:TEMP\ExitCodeTasks"
New-Item -Path $taskScriptsPath -ItemType Directory -Force | Out-Null


@(
    @{ Name = "TaskExit0"; Code = 0 },
    @{ Name = "TaskExit1"; Code = 1 },
    @{ Name = "TaskExit2"; Code = 2 }
) | ForEach-Object {
    $taskName = $_.Name
    $exitCode = $_.Code
    $batFile = "$taskScriptsPath\$taskName.bat"

    # Create the batch file
    Set-Content -Path $batFile -Value "exit $exitCode"

    # Round up to next full minute
    $nextMinute = (Get-Date).AddMinutes(1)
    $startTime = $nextMinute.ToString("HH:mm")

    # Schedule the task
    schtasks /Create /TN $taskName /TR "`"cmd /c $batFile`"" /SC ONCE /ST $startTime /F /RL LIMITED /RU "$env:USERNAME"

    # Start the task immediately
    Start-ScheduledTask -TaskName $taskName
}


$json_test_param = $test_param | ConvertTo-Json -Compress

Write-Host "json_test_param" $json_test_param
$quoted_json_test_param = "'" + $json_test_param + "'"

wsl cd $wsl_path `&`& .github/scripts/wsl-collect-test-robot.sh broker-engine/$robot $quoted_json_test_param

#something wrong in robot test => exit 1 => failure
if (Test-Path -Path 'reports\windows-cma-failed' -PathType Container) {
    exit 1
}



