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

param($compile_ut="On")

<#
.SYNOPSIS
add builtin revision to vcpkg.json

.DESCRIPTION
linux and windows compil does not use the same builtin revision, so we can not store it in vcpkg.json
The purpose of this function is to add it in vcpkg.json before compile dependencies
#>
function add_builtin_to_vcpkg_json {
    $content = Get-Content "vcpkg.json"
    $new_content = @()
    $inserted = 0

    foreach ($line in $content) {
        $newContent += $line
        if ($line -match "{" -and $inserted -eq 0) {
            $newContent += '"builtin-baseline":"ef7dbf94b9198bc58f45951adcf1f041fcbc5ea0", ' #2025.06.13
            $inserted = 1
        }
    }

    $newContent | Set-Content "vcpkg.json"

    #    Write-Host "new content of vcpkg.json:"
    #    Write-Host "*********************************************************************************"
    #    Get-Content "vcpkg.json"
    #    Write-Host "*********************************************************************************"
}



Write-Host "Work in" $pwd.ToString()

[System.Environment]::SetEnvironmentVariable("AWS_EC2_METADATA_DISABLED", "true")

Write-Host $env:VCPKG_BINARY_SOURCES

$current_dir = $pwd.ToString()

#install recent version of 7zip needed by some packages
Write-Host "install 7zip"

#download 7zip
Invoke-WebRequest -Uri "https://www.7-zip.org/a/7z2408-x64.msi" -OutFile "7z2408-x64.msi"
#install 7zip
Start-Process 'msiexec.exe' -ArgumentList '/I "7z2408-x64.msi" /qn' -Wait

#set builtin here in order to not impact linux compil
add_builtin_to_vcpkg_json

#get cache from s3
$files_to_hash = "vcpkg.json", "custom-triplets\x64-windows.cmake", "CMakeLists.txt", "CMakeListsWindows.txt"
$files_content = Get-Content -Path $files_to_hash -Raw
$stringAsStream = [System.IO.MemoryStream]::new()
$writer = [System.IO.StreamWriter]::new($stringAsStream)
$writer.write($files_content -join " ")
$writer.Flush()
$stringAsStream.Position = 0
$vcpkg_hash = Get-FileHash -InputStream $stringAsStream -Algorithm SHA256 | Select-Object Hash
$file_name = "windows-agent-vcpkg-dependencies-cache-" + $vcpkg_hash.Hash + "-" + $vcpkg_release
$file_name_extension = "${file_name}.7z"

#try to get compiled dependenciesfrom s3
Write-Host "try to download compiled dependencies from s3: $file_name_extension"
aws --quiet s3 cp s3://centreon-collect-robot-report/$file_name_extension $file_name_extension
if ( $? -ne $true ) {
    #no => generate
    Write-Host "#######################################################################################################################"
    Write-Host "compiled dependencies unavailable for this version we will need to build it, it will take a long time"
    Write-Host "#######################################################################################################################"

    Write-Host "install vcpkg"
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    bootstrap-vcpkg.bat
    cd $current_dir

    [System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", $pwd.ToString() + "\vcpkg")
    [System.Environment]::SetEnvironmentVariable("PATH", $pwd.ToString() + "\vcpkg;" + $env:PATH)

    Write-Host "compile vcpkg dependencies"
    vcpkg install --vcpkg-root $env:VCPKG_ROOT  --x-install-root build_windows\vcpkg_installed --x-manifest-root . --overlay-triplets custom-triplets --triplet x64-windows --overlay-ports=overlays-windows

    if ( $? -eq $true ) {
        Write-Host "Compress binary archive"
        7z a $file_name_extension  build_windows\vcpkg_installed
        Write-Host "Upload binary archive"
        aws s3 cp $file_name_extension s3://centreon-collect-robot-report/$file_name_extension
    }
}
else {
    7z x $file_name_extension
    Write-Host "Create cmake files from binary-cache downloaded without use vcpkg"
}

Write-Host "create CMake files with cmake -DCMAKE_BUILD_TYPE=Release -DWITH_TESTING=$compile_ut -DWINDOWS=On -DBUILD_FROM_CACHE=On -S. -DVCPKG_CRT_LINKAGE=static -DVCPKG_LIBRARY_LINKAGE=static -DVCPKG_BUILD_TYPE=release -DBUILD_SHARED_LIBS=OFF -Bbuild_windows"

cmake -DCMAKE_BUILD_TYPE=Release "-DWITH_TESTING=$compile_ut" -DWINDOWS=On -DBUILD_FROM_CACHE=On -S. -DVCPKG_CRT_LINKAGE=static -DVCPKG_LIBRARY_LINKAGE=static -DVCPKG_BUILD_TYPE=release -DBUILD_SHARED_LIBS=OFF -Bbuild_windows

#Write-Host "------------- vcpkg used compiler ---------------"
#Get-Content "build_windows\vcpkg_installed\vcpkg\compiler-file-hash-cache.json"
#Write-Host "------------- build agent only ---------------"

cmake --build build_windows --config Release

