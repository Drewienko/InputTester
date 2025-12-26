param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Deploy
)

$qtPrefixPath = $env:QT_PREFIX_PATH
if ([string]::IsNullOrWhiteSpace($qtPrefixPath)) 
{
    $qtPrefixPath = "C:\Qt\6.10.1\msvc2022_64"
}

$buildDir = "build-win-$Configuration"

cmake -S . -B $buildDir -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="$qtPrefixPath"
cmake --build $buildDir --config $Configuration

if ($Deploy) 
{
    $deployTarget = "deployQtKeyLog"
    if ($Configuration -eq "Debug") 
    {
        $deployTarget = "deployQtKeyLogDebug"
    }
    cmake --build $buildDir --config $Configuration --target $deployTarget
}
