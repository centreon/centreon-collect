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

$id=$args[1]
$project_path = $args[2]

$status = -1
$state_file = $project_path + "/tests/states"
if (Test-Path -Path $state_file) {
  $status = select-string -path $state_file -pattern "${id}=>(\d+)" | %{$_.Matches[0].Groups[1].Value }
}

$szd = Get-Date -UFormat "%s"
$dd =  [double]$szd
$d = [int]($dd / 1000000)

$d = [int]($d + 3 * $id) -band 0x1ff;

$d = $d / ($id + 1);
$w = 300 / ($id + 1);
$c = 400 / ($id + 1);
if ($status -eq 0) {
  $d = $w / 2;
} elseif ($status -eq 1) {
  $d = ($w + $c) / 2;
} elseif ($status -eq 2) {
  $d = 2 * $c;
} else {
  if ($d -gt $c) {
    $status = 2;
  } elseif ($d -gt $w) {
    $status = 1;
  } else {
    $status = 0;
  }
}

"Test check $id | metric={0:n2};{1:n2};{2:n2}" -f $d, $w, $c

exit $status;
