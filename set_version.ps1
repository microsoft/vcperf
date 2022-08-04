param (
    [String]$BuildNumber = ""
)

$VersionMajor = "2"
$VersionMinor = "2"

Write-Host "##vso[task.setvariable variable=VersionString;]$VersionMajor.$VersionMinor.$BuildNumber"
