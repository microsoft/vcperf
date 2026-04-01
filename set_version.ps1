param (
    [String]$BuildNumber = ""
)

$VersionMajor = "2"
$VersionMinor = "7"

Write-Host "##vso[task.setvariable variable=VersionString;]$VersionMajor.$VersionMinor.$BuildNumber"
