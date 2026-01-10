# UFT Windows Release Package Builder
# Creates a distributable ZIP with all dependencies
#
# Usage: .\build_release.ps1 [-Version "3.7.0"]

param(
    [string]$Version = "3.7.0"
)

$ErrorActionPreference = "Stop"

Write-Host "════════════════════════════════════════════════════════════"
Write-Host " UFT Windows Release Builder"
Write-Host " Version: $Version"
Write-Host "════════════════════════════════════════════════════════════"

$BuildDir = "build"
$ReleaseDir = "release"
$PackageName = "uft-$Version-windows-x64"

# Clean release directory
if (Test-Path $ReleaseDir) {
    Remove-Item -Recurse -Force $ReleaseDir
}
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null

# Build if needed
if (-not (Test-Path "$BuildDir/Release/UnifiedFloppyTool.exe")) {
    Write-Host "Building UFT..."
    cmake -B $BuildDir -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    cmake --build $BuildDir --config Release --parallel
}

# Copy main executable
$ExePath = "$BuildDir/Release/UnifiedFloppyTool.exe"
if (Test-Path $ExePath) {
    Write-Host "Copying executable..."
    Copy-Item $ExePath "$ReleaseDir/"
    
    # Deploy Qt dependencies
    Write-Host "Deploying Qt dependencies..."
    $QtBinPath = (Get-Command qmake -ErrorAction SilentlyContinue).Path | Split-Path
    if ($QtBinPath) {
        & "$QtBinPath/windeployqt.exe" "$ReleaseDir/UnifiedFloppyTool.exe" --release --no-translations
    }
} else {
    Write-Host "Warning: Main executable not found (GUI may be disabled)"
}

# Copy documentation
Write-Host "Copying documentation..."
Copy-Item "README.md" "$ReleaseDir/"
Copy-Item "CHANGELOG.md" "$ReleaseDir/"
if (Test-Path "BUILD_INSTRUCTIONS.md") {
    Copy-Item "BUILD_INSTRUCTIONS.md" "$ReleaseDir/"
}

# Create version info file
@"
UnifiedFloppyTool v$Version
Windows x64 Release

Build Date: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Build Type: Release

For installation instructions, see README.md
For changes in this version, see CHANGELOG.md
"@ | Out-File -FilePath "$ReleaseDir/VERSION.txt" -Encoding UTF8

# Create ZIP archive
Write-Host "Creating ZIP archive..."
$ZipPath = "$PackageName.zip"
if (Test-Path $ZipPath) {
    Remove-Item $ZipPath
}
Compress-Archive -Path "$ReleaseDir/*" -DestinationPath $ZipPath

# Calculate sizes
$ZipSize = (Get-Item $ZipPath).Length / 1MB
$FileCount = (Get-ChildItem -Path $ReleaseDir -Recurse -File).Count

Write-Host ""
Write-Host "════════════════════════════════════════════════════════════"
Write-Host " Package created: $ZipPath"
Write-Host " Size: $([math]::Round($ZipSize, 2)) MB"
Write-Host " Files: $FileCount"
Write-Host "════════════════════════════════════════════════════════════"

# Cleanup
Remove-Item -Recurse -Force $ReleaseDir

Write-Host ""
Write-Host "Done!"
