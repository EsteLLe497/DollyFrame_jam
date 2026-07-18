# =========================================================
# ファイルの情報[package_distribution.ps1]
#
# 制作者:Masatora Tanaka		日付：2026/07/18
# =========================================================

param(
    [string]$configuration = "Debug",
    [string]$packageName = "DollyFrame",
    [switch]$createZip,
    [switch]$hideAssets = $true
)

$ErrorActionPreference = "Stop"

# =========================================================
# 初期設定
# =========================================================
$rootDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildDir = Join-Path $rootDir "build\$configuration"
$distRoot = Join-Path $rootDir "dist"
$packageDir = Join-Path $distRoot $packageName
$exeSource = Join-Path $buildDir "DirectXFoundation.exe"
$exeTarget = Join-Path $packageDir "DollyFrame.exe"
$assetsSource = Join-Path $rootDir "assets"
$assetsTarget = Join-Path $packageDir "assets"
$readmePath = Join-Path $packageDir "README.txt"
$zipPath = Join-Path $distRoot "$packageName.zip"

# =========================================================
# 関数説明（ex 配布フォルダの再作成
# =========================================================
function resetPackageDirectory
{
    if (Test-Path $packageDir)
    {
        attrib -h $assetsTarget /s /d 2>$null
        Remove-Item -LiteralPath $packageDir -Recurse -Force
    }

    New-Item -ItemType Directory -Path $packageDir | Out-Null
}

# =========================================================
# 関数説明（ex ファイルコピー
# =========================================================
function copyRuntimeFiles
{
    if (-not (Test-Path $exeSource))
    {
        throw "実行ファイルが見つかりません: $exeSource"
    }

    if (-not (Test-Path $assetsSource))
    {
        throw "assetsフォルダが見つかりません: $assetsSource"
    }

    Copy-Item -LiteralPath $exeSource -Destination $exeTarget -Force
    Copy-Item -LiteralPath $assetsSource -Destination $assetsTarget -Recurse -Force
}

# =========================================================
# 関数説明（ex 素材フォルダの隠し属性設定
# =========================================================
function applyAssetVisibility
{
    if (-not $hideAssets)
    {
        return
    }

    attrib +h $assetsTarget /s /d
}

# =========================================================
# 関数説明（ex 配布用メモ作成
# =========================================================
function writePackageReadme
{
    $readmeText = @'
DollyFrame

Run:
  DollyFrame.exe

Included:
  DollyFrame.exe
  assets

Note:
  The assets folder is required at runtime.
  It is marked as hidden, but the game will not run correctly if it is removed.
'@

    Set-Content -Path $readmePath -Value $readmeText -Encoding UTF8
}

# =========================================================
# 関数説明（ex zip作成
# =========================================================
function createPackageZip
{
    if (-not $createZip)
    {
        return
    }

    if (Test-Path $zipPath)
    {
        Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -Force
}

# =========================================================
# 関数説明（ex メイン処理
# =========================================================
resetPackageDirectory
copyRuntimeFiles
writePackageReadme
applyAssetVisibility
createPackageZip

Write-Host "Package created: $packageDir"
if ($createZip)
{
    Write-Host "Zip created: $zipPath"
}
