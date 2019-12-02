param (
    [String]$VersionString = "",
    [String]$SHA1= ""
)

#clean local tags
git fetch -q --prune origin "+refs/tags/*:refs/tags/*"
git tag $VersionString $sha1
git push -q origin --tags