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

# This script is used to measure the performance of the agent cma or nsclient

param (
    [Parameter(Mandatory = $true)]
    [string]$process_name
)

try {
    $process = Get-Process -Name $process_name    
}
catch {
    Write-Host "CRITICAL: Process $process_name not found"
    exit 2
}


if ($process -eq $null) {
    Write-Host "CRITICAL: Process $process_name not found"
    exit 2
}

$nb_thread = $process.Threads.Count[0]
$cpu = $process.TotalProcessorTime.TotalSeconds
$kernel_cpu = $process.PrivilegedProcessorTime.TotalSeconds[0]
$memory = $process.WorkingSet64[0] / 1024 / 1024
$handle_count = $process.HandleCount[0]
$time_from_start = (Get-Date) - $process.StartTime
$total_cpu= $cpu  * 100 / $time_from_start.TotalSeconds

Write-Host "OK: $process_name | threads=$nb_thread;;;; cpu=$cpu;;;; kernel_cpu=$kernel_cpu;;;; memory=${memory}M;;;; handle_count=$handle_count;;;; time_from_start=$time_from_start total_cpu=$total_cpu%;;;;"

exit 0
