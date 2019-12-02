param (
    [String]$BuildNumber = ""
)

$VersionMajor = "1"
$VersionMinor = "3"

Write-Host "##vso[task.setvariable variable=VersionString;]$VersionMajor.$VersionMinor.$BuildNumber"