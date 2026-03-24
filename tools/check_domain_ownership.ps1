param(
    [string[]]$Files = @(),
    [switch]$Staged
)

$ErrorActionPreference = "Stop"

function Get-ChangedFiles {
    param([switch]$UseStaged)

    if ($UseStaged) {
        $out = git diff --cached --name-only
    }
    else {
        $out = git diff --name-only
    }
    return @($out | Where-Object { $_ -and $_.Trim().Length -gt 0 })
}

if (-not $Files -or $Files.Count -eq 0) {
    $Files = Get-ChangedFiles -UseStaged:$Staged
}
elseif ($Files.Count -eq 1 -and $Files[0] -like "*,*") {
    $Files = @($Files[0].Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}

if (-not $Files -or $Files.Count -eq 0) {
    Write-Host "No changed files to validate."
    exit 0
}

$domainMatchers = @(
    @{ Name = "player"; Patterns = @("^scenes/game/systems/player/", "^scenes/game/game_scene_player_") },
    @{ Name = "capture"; Patterns = @("^scenes/game/systems/capture/", "^gameplay/photo/capture/", "^gameplay/photo_capture_system\.(h|cpp)$", "^gameplay/photo_system_capture\.cpp$") },
    @{ Name = "paste"; Patterns = @("^scenes/game/systems/paste/", "^gameplay/photo/paste/", "^gameplay/photo_paste_system\.(h|cpp)$", "^gameplay/photo_system_paste\.cpp$") },
    @{ Name = "filter"; Patterns = @("^scenes/game/filter/", "^scenes/game/game_scene_update_domains\.cpp$", "^gameplay/photo_filter_rules\.(h|cpp)$") },
    @{ Name = "enemy"; Patterns = @("^scenes/game/systems/enemy/", "^scenes/game/game_scene_enemy_domain\.cpp$", "^scenes/game/game_scene_combat_system\.h$") },
    @{ Name = "stage_gimmick"; Patterns = @("^scenes/game/systems/stage_gimmick/", "^scenes/game/game_scene_gimmick_domain\.cpp$", "^scenes/game/game_scene_world_interaction_system\.h$") }
)

$sharedPatterns = @(
    "^scenes/game/game_scene\.cpp$",
    "^scenes/game/game_scene\.h$",
    "^scenes/game/game_scene_gameplay\.cpp$",
    "^scenes/game/game_scene_collision\.cpp$",
    "^scenes/game/game_scene_internal\.h$",
    "^gameplay/components\.(h|cpp)$",
    "^gameplay/photo_system\.(h|cpp)$",
    "^gameplay/photo_system_bridge\.h$",
    "^gameplay/photo_shared\.(h|cpp)$",
    "^docs/"
)

$touchedDomains = New-Object System.Collections.Generic.HashSet[string]
$unknownFiles = @()

foreach ($file in $Files) {
    $normalized = $file -replace "\\", "/"
    $matched = $false

    foreach ($domain in $domainMatchers) {
        foreach ($pattern in $domain.Patterns) {
            if ($normalized -match $pattern) {
                $null = $touchedDomains.Add($domain.Name)
                $matched = $true
                break
            }
        }
        if ($matched) { break }
    }

    if ($matched) { continue }

    foreach ($pattern in $sharedPatterns) {
        if ($normalized -match $pattern) {
            $matched = $true
            break
        }
    }

    if (-not $matched) {
        $unknownFiles += $normalized
    }
}

if ($unknownFiles.Count -gt 0) {
    Write-Host "Ownership check warning: files outside domain/shared map found:"
    $unknownFiles | ForEach-Object { Write-Host "  - $_" }
    Write-Host "Please classify these paths in tools/check_domain_ownership.ps1 if needed."
}

$domainList = @($touchedDomains)
if ($domainList.Count -le 1) {
    if ($domainList.Count -eq 1) {
        Write-Host "Ownership check passed: touched domain = $($domainList[0])"
    }
    else {
        Write-Host "Ownership check passed: shared/docs-only changes."
    }
    exit 0
}

Write-Error ("Ownership check failed: multiple domains touched in one change set: " + ($domainList -join ", "))
