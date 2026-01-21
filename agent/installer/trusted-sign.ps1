param(
  [Parameter(Mandatory=$true)]
  [string] $FileToSign
)

# Ensure module exists (action installs it; this makes local builds work too)
$wantedVersion = "0.5.8"
if (-not (Get-Module -ListAvailable -Name TrustedSigning | Where-Object Version -eq $wantedVersion)) {
  Install-Module -Name TrustedSigning -RequiredVersion $wantedVersion -Force -Scope CurrentUser -Repository PSGallery
}
Import-Module TrustedSigning -RequiredVersion $wantedVersion -ErrorAction Stop

Invoke-TrustedSigning `
  -Endpoint "https://weu.codesigning.azure.net/" `
  -CodeSigningAccountName "Centreon-signature-RD" `
  -CertificateProfileName "Cert-Signature-RD" `
  -Files $FileToSign `
  -FileDigest "SHA256" `
  -TimestampRfc3161 "http://timestamp.acs.microsoft.com" `
  -TimestampDigest "SHA256" `
  -ExcludeInteractiveBrowserCredential
