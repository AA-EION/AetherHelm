<#
.SYNOPSIS
  Installs AetherHelm MCP Server into Claude Desktop, Cursor, Windsurf, and Cline configurations.
#>

$ErrorActionPreference = "Stop"

Write-Host "=== AetherHelm MCP Auto-Configuration (Windows) ===" -ForegroundColor Cyan

# Locate executable
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir
$Candidates = @(
    "$RepoRoot\build\aetherhelm-mcp.exe",
    "$RepoRoot\build\Release\aetherhelm-mcp.exe",
    "$RepoRoot\build\aetherhelm-mcp_artefacts\Release\aetherhelm-mcp.exe",
    "$RepoRoot\build\aetherhelm-mcp_artefacts\aetherhelm-mcp.exe",
    "$RepoRoot\bin\aetherhelm-mcp.exe",
    (Get-Command aetherhelm-mcp -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
)

$McpExe = $null
foreach ($c in $Candidates) {
    if ($c -and (Test-Path $c)) {
        $McpExe = (Resolve-Path $c).Path
        break
    }
}

if (-not $McpExe) {
    $McpExe = "$RepoRoot\build\aetherhelm-mcp.exe"
    Write-Host "[!] Note: Executable not yet compiled at $McpExe. Registering target path." -ForegroundColor Yellow
} else {
    Write-Host "[+] Found AetherHelm MCP binary at: $McpExe" -ForegroundColor Green
}

function Update-JsonConfig {
    param (
        [string]$Path,
        [string]$Name
    )

    $Dir = Split-Path -Parent $Path
    if (-not (Test-Path $Dir)) {
        Write-Host "[-] Skipping $Name (Directory does not exist: $Dir)" -ForegroundColor DarkGray
        return
    }

    $Json = @{}
    if (Test-Path $Path) {
        try {
            $Content = Get-Content $Path -Raw
            if ($Content.Trim().Length -gt 0) {
                $Json = $Content | ConvertFrom-Json -AsHashtable
            }
        } catch {
            Write-Host "[!] Warning reading existing $Path: $_" -ForegroundColor Yellow
            $Json = @{}
        }
    }

    if (-not $Json.ContainsKey("mcpServers")) {
        $Json["mcpServers"] = @{}
    }

    $Json["mcpServers"]["aetherhelm"] = @{
        "command" = $McpExe
        "args" = @()
    }

    $Json | ConvertTo-Json -Depth 10 | Set-Content -Path $Path -Encoding utf8
    Write-Host "[+] Configured $Name -> $Path" -ForegroundColor Green
}

# 1. Claude Desktop
$ClaudePath = "$env:APPDATA\Claude\claude_desktop_config.json"
Update-JsonConfig -Path $ClaudePath -Name "Claude Desktop"

# 2. Cursor
$CursorPath = "$env:USERPROFILE\.cursor\mcp.json"
Update-JsonConfig -Path $CursorPath -Name "Cursor"

# 3. Windsurf
$WindsurfPath = "$env:USERPROFILE\.codeium\windsurf\mcp_config.json"
Update-JsonConfig -Path $WindsurfPath -Name "Windsurf"

# 4. Cline / Roo-Code
$ClinePath = "$env:APPDATA\Code\User\globalStorage\saoudrizwan.claude-dev\settings\cline_mcp_settings.json"
Update-JsonConfig -Path $ClinePath -Name "Cline"

$RooPath = "$env:APPDATA\Code\User\globalStorage\rooveterinaryinc.roo-cline\settings\cline_mcp_settings.json"
Update-JsonConfig -Path $RooPath -Name "Roo-Code"

Write-Host "`n[✓] AetherHelm MCP Server configuration complete!" -ForegroundColor Cyan
