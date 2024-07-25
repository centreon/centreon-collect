$current_dir = (pwd).Path
$wsl_path =  "/mnt/" + $current_dir.SubString(0,1).ToLower() + "/" + $current_dir.SubString(3).replace('\','/')


reg import agent/conf/centagent.reg


#in wsl1, no VM, so IP address are identical in host and wsl

$my_ip = (Get-NetIpAddress -AddressFamily IPv4 | Where-Object IPAddress -ne "127.0.0.1" | SELECT IPAddress -First 1).IPAddress

Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name endpoint -Value ${my_ip}:4317
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name reversed_grpc_streaming -Value 0

#Start agent
$agent_process = Start-Process -PassThru -FilePath build_windows\agent\Release\centagent.exe

if ($agent_process.exitdsfsfsf)

Start-Sleep -Seconds 1

#Start reverse agent
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name endpoint -Value 0.0.0.0:4320
Set-ItemProperty -Path HKLM:\SOFTWARE\Centreon\CentreonMonitoringAgent  -Name reversed_grpc_streaming -Value 1
$reversed_agent_process = Start-Process -PassThru -FilePath build_windows\agent\Release\centagent.exe
if ($reversed_agent_process.exitdsfsfsf)


wsl cd $wsl_path `&`& .github/scripts/wsl-collect-test-robot.sh broker-engine/cma.robot $my_ip

Stop-Process -InputObject $agent_process
Stop-Process -InputObject $reversed_agent_process


