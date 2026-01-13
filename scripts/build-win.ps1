param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Deploy,
    [string]$QtPrefix
)

$preset = "windows-release-msvc"
if ($Configuration -eq "Debug") {
    $preset = "windows-debug-msvc"
}

$qtPrefixValue = if ($QtPrefix) { $QtPrefix } elseif ($env:QT6_PREFIX) { $env:QT6_PREFIX } else { "C:/Qt/6.10.1/msvc2022_64" }
$env:QT6_PREFIX = $qtPrefixValue

cmake --preset $preset
cmake --build --preset $preset

if ($Deploy) 
{
    $deployPreset = "$preset-deploy"
    cmake --build --preset $deployPreset
}
