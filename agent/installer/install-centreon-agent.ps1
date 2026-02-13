#Requires -Version 5.1

<#
.SYNOPSIS
    Download and install Centreon Monitoring Agent
.DESCRIPTION
    This script downloads the latest Centreon Monitoring Agent installer from GitHub
    and executes it with the specified parameters.
.PARAMETER Version
    The version tag to use. Available options: "25.10-latest" or "24.10-latest" (default: "25.10-latest")
.PARAMETER Token
    Authentication token. If not provided or empty, the script will prompt for it.
.PARAMETER Endpoint
    Agent endpoint URL
.PARAMETER HostName
    Host name (if not provided, uses Windows hostname)
.PARAMETER Components
    Components to install (default: "agent,plugins" - both are installed if not specified)
.PARAMETER CommonName
    Common name for certificate
.PARAMETER Fingerprint
    Certificate fingerprint
.PARAMETER Reverse
    Enable reverse connection
.PARAMETER Encryption
    Enable encryption
.PARAMETER Cert
    Certificate file path
.PARAMETER Key
    Private key file path
.PARAMETER CA
    CA certificate file path
.PARAMETER LogType
    Log type (file, eventlog, etc.)
.PARAMETER LogFile
    Log file path
.PARAMETER LogLevel
    Log level (trace, debug, info, warning, error, critical)
.PARAMETER MaxFileSize
    Maximum log file size
.PARAMETER MaxNumber
    Maximum number of log files
.PARAMETER CustomCheck
    Custom check configuration
.PARAMETER PluginSrc
    Plugin source path
.EXAMPLE
    .\install-centreon-agent.ps1 -Endpoint "h:1"
    Minimal installation with just endpoint and token (prompted). Host defaults to Windows hostname, both agent and plugins installed.
.EXAMPLE
    .\install-centreon-agent.ps1 -Token "mytoken" -Endpoint "https://centreon.example.com"
    Installation with token provided as parameter, using default hostname and components.
.EXAMPLE
    .\install-centreon-agent.ps1 -Token "mytoken" -Endpoint "https://centreon.example.com" -HostName "myhost" -LogLevel "info"
    Full installation with custom hostname and log level.
.EXAMPLE
    .\install-centreon-agent.ps1 -Version "24.10-latest" -Endpoint "https://centreon.example.com" -HostName "myhost"
    Installation using a specific version (24.10-latest instead of default 25.10-latest).
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [string]$Version = "25.10",
    
    [Parameter(Mandatory=$true)]
    [string]$Token = "",
    
    [Parameter(Mandatory=$true)]
    [string]$Endpoint = "",
    
    [Parameter(Mandatory=$false)]
    [string]$HostName = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Components = "",
    
    [Parameter(Mandatory=$false)]
    [string]$CommonName = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Fingerprint = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Reverse = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Encryption = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Cert = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Key = "",
    
    [Parameter(Mandatory=$false)]
    [string]$CA = "",
    
    [Parameter(Mandatory=$false)]
    [string]$LogType = "",
    
    [Parameter(Mandatory=$false)]
    [string]$LogFile = "",
    
    [Parameter(Mandatory=$false)]
    [string]$LogLevel = "",
    
    [Parameter(Mandatory=$false)]
    [string]$MaxFileSize = "",
    
    [Parameter(Mandatory=$false)]
    [string]$MaxNumber = "",
    
    [Parameter(Mandatory=$false)]
    [string]$CustomCheck = "",
    
    [Parameter(Mandatory=$false)]
    [string]$PluginSrc = ""
    
)

# GitHub repository information
$GitHubOwner = "centreon"
$GitHubRepo = "centreon-collect"
$TempDir = Join-Path $env:TEMP "centreon-agent-installer"

# Function to write log messages
function Write-Log {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] [$Level] $Message"
}

# Function to get the CMA version from a tag
function Get-CMAVersion {
    param(
        [string]$TagName
    )
    
    try {
        Write-Log "Fetching tag information for: $TagName"
        
        # Append "-latest" if not already present
        if (-not $TagName.EndsWith("-latest")) {
            $TagName = "$TagName-latest"
            Write-Log "Using tag: $TagName"
        }
        
        # Get the commit SHA for the tag
        $tagUrl = "https://api.github.com/repos/$GitHubOwner/$GitHubRepo/git/refs/tags/$TagName"
        Write-Log "Tag URL: $tagUrl"
        $tagResponse = Invoke-RestMethod -Uri $tagUrl -Method Get -Headers @{
            "Accept" = "application/vnd.github.v3+json"
            "User-Agent" = "PowerShell-CentreonInstaller"
        }
        $commitSha = $tagResponse.object.sha

        Write-Log "Commit SHA: $commitSha"
        
        # Get all tags for this commit (with pagination)
        $tagsUrl = "https://api.github.com/repos/$GitHubOwner/$GitHubRepo/tags"
        Write-Log "Tags URL: $tagsUrl"
        $allTags = @()
        $page = 1
        $perPage = 100

        # Loop to fetch all tags with pagination
        do {
            $paginatedUrl = "$tagsUrl" + "?per_page=$perPage" + "&page=$page"
            Write-Log "Fetching tags page $page from: $paginatedUrl"
            
            try {
                $tagsResponse = Invoke-RestMethod -Uri $paginatedUrl -Method Get -Headers @{
                    "Accept" = "application/vnd.github.v3+json"
                    "User-Agent" = "PowerShell-CentreonInstaller"
                }
            }
            catch {
                Write-Log "Error fetching page $page : $_" -Level "ERROR"
                throw
            }
            
            if ($tagsResponse.Count -gt 0) {
                $allTags += $tagsResponse
                $page++
            }
        } while ($tagsResponse.Count -eq $perPage)
        
        Write-Log "Total tags fetched: $($allTags.Count)"
        
        # Find all tags for this commit
        $matchingTags = $allTags | Where-Object { $_.commit.sha -eq $commitSha }
        
        if ($matchingTags) {
            Write-Log "Tags found for commit $commitSha :"
            foreach ($tag in $matchingTags) {
                Write-Log "  - $($tag.name)"
            }
        } else {
            Write-Log "No tags found for commit $commitSha" -Level "WARNING"
        }
        
        # Find the centreon-monitoring-agent tag
        $cmaTag = $matchingTags | Where-Object { 
            $_.name -like "centreon-monitoring-agent-*"
        } | Select-Object -First 1
        
        if ($null -eq $cmaTag) {
            throw "Could not find centreon-monitoring-agent tag for version $TagName"
        }
        
        $cmaVersion = $cmaTag.name
        Write-Log "Found CMA version: $cmaVersion" -Level "SUCCESS"
        
        return $cmaVersion
    }
    catch {
        Write-Log "Error getting CMA version: $_" -Level "ERROR"
        throw
    }
}

# Function to download the installer
function Download-Installer {
    param(
        [string]$CMAVersion
    )
    
    try {
        # Extract version number from tag (e.g., centreon-monitoring-agent-25.10.1 -> 25.10.1)
        if ($CMAVersion -match "centreon-monitoring-agent-(.+)") {
            $versionNumber = $Matches[1]
        } else {
            throw "Invalid CMA version format: $CMAVersion"
        }
        
        # Construct download URL
        $downloadUrl = "https://github.com/$GitHubOwner/$GitHubRepo/releases/download/$CMAVersion/centreon-monitoring-agent-$versionNumber.exe"
        $installerPath = Join-Path $TempDir "centreon-monitoring-agent-$versionNumber.exe"
        
        Write-Log "Download URL: $downloadUrl"
        Write-Log "Installer path: $installerPath"
        
        # Create temp directory if it doesn't exist
        if (-not (Test-Path $TempDir)) {
            New-Item -ItemType Directory -Path $TempDir | Out-Null
        }
        
        # Download the installer with retry logic
        Write-Log "Downloading installer..."
        $maxRetries = 3
        $retryCount = 0
        $downloadSuccess = $false
        
        while (-not $downloadSuccess -and $retryCount -lt $maxRetries) {
            try {
                if ($retryCount -gt 0) {
                    $waitTime = [Math]::Pow(2, $retryCount)
                    Write-Log "Retry attempt $retryCount of $maxRetries after $waitTime seconds..." -Level "WARNING"
                    Start-Sleep -Seconds $waitTime
                }
                
                $ProgressPreference = 'SilentlyContinue'
                Invoke-WebRequest -Uri $downloadUrl -OutFile $installerPath -UseBasicParsing -TimeoutSec 300
                $ProgressPreference = 'Continue'
                
                if (Test-Path $installerPath) {
                    $fileSize = (Get-Item $installerPath).Length
                    if ($fileSize -gt 0) {
                        $fileSizeMB = $fileSize / 1MB
                        Write-Log "Installer downloaded successfully ($([math]::Round($fileSizeMB, 2)) MB)" -Level "SUCCESS"
                        $downloadSuccess = $true
                    } else {
                        throw "Downloaded file is empty"
                    }
                }
            }
            catch {
                $retryCount++
                if ($retryCount -ge $maxRetries) {
                    throw "Download failed after $maxRetries attempts: $_"
                }
                Write-Log "Download attempt failed: $_" -Level "WARNING"
            }
        }
        
        if (-not $downloadSuccess) {
            throw "Failed to download installer after $maxRetries attempts"
        }
        
        return $installerPath
    }
    catch {
        Write-Log "Error downloading installer: $_" -Level "ERROR"
        throw
    }
}

# Function to build installer arguments
function Build-InstallerArgs {
    $args = @()
    
    # Add silent installation flag
    $args += "/VERYSILENT"
    
    # Add all other parameters if they are provided (in key=value format)
    if ($Endpoint) { $args += "/ENDPOINT=$Endpoint" }
    if ($HostName) { $args += "/HOST=$HostName" }
    if ($Components) { $args += "/COMPONENTS=$Components" }
    if ($CommonName) { $args += "/COMMONNAME=$CommonName" }
    if ($Fingerprint) { $args += "/FINGERPRINT=$Fingerprint" }
    if ($Token) { $args += "/TOKEN=$Token" }
    if ($Reverse) { $args += "/REVERSE=$Reverse" }
    if ($Encryption) { $args += "/ENCRYPTION=$Encryption" }
    if ($Cert) { $args += "/CERT=$Cert" }
    if ($Key) { $args += "/KEY=$Key" }
    if ($CA) { $args += "/CA=$CA" }
    if ($LogType) { $args += "/LOGTYPE=$LogType" }
    if ($LogFile) { $args += "/LOGFILE=$LogFile" }
    if ($LogLevel) { $args += "/LOGLEVEL=$LogLevel" }
    if ($MaxFileSize) { $args += "/MAXFILESIZE=$MaxFileSize" }
    if ($MaxNumber) { $args += "/MAXNUMBER=$MaxNumber" }
    if ($CustomCheck) { $args += "/CUSTOMCHECKFILE=$CustomCheck" }
    if ($PluginSrc) { $args += "/PLUGINSRC=$PluginSrc" }
    
    return $args
}

# Function to execute the installer
function Install-CentreonAgent {
    param(
        [string]$InstallerPath,
        [array]$Arguments
    )
    
    try {
        Write-Log "Starting installer..."
        Write-Log "Command: $InstallerPath $($Arguments -join ' ')"
        
        $process = Start-Process -FilePath $InstallerPath -ArgumentList $Arguments -Wait -PassThru -NoNewWindow
        
        if ($process.ExitCode -eq 0) {
            Write-Log "Installation completed successfully" -Level "SUCCESS"
        } else {
            Write-Log "Installation completed with exit code: $($process.ExitCode)" -Level "ERROR"
            # Read the log file installer_output.log if it exists
            $logFilePath = Join-Path $TempDir "installer_output.log"
            if (Test-Path $logFilePath) {
                Write-Log "Installer log output:" -Level "INFO"
                Get-Content $logFilePath | ForEach-Object { Write-Log $_ -Level "ERROR" }
            }
        }
        
        return $process.ExitCode
    }
    catch {
        Write-Log "Error executing installer: $_" -Level "ERROR"
        throw
    }
}

# Main script execution
try {
    Write-Log "=== Centreon Monitoring Agent Installation Script ===" -Level "INFO"
    Write-Log "Version tag: $Version"
    
    # Get CMA version from GitHub
    $cmaVersion = Get-CMAVersion -TagName $Version
    
    # Download installer
    $installerPath = Download-Installer -CMAVersion $cmaVersion
    
    # Set hostname to machine name if not provided
    if ([string]::IsNullOrWhiteSpace($HostName)) {
        $HostName = $env:COMPUTERNAME
        Write-Log "No hostname provided, using machine name: $HostName"
    }
    
    # Build installer arguments
    $installerArgs = Build-InstallerArgs
    
    # Execute installer
    $exitCode = Install-CentreonAgent -InstallerPath $installerPath -Arguments $installerArgs
    
    Write-Log "=== Installation Process Completed ===" -Level "INFO"
    exit $exitCode
}
catch {
    Write-Log "Fatal error: $_" -Level "ERROR"
    Write-Log $_.ScriptStackTrace -Level "ERROR"
    exit 1
}
finally {

}
