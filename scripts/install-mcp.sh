#!/usr/bin/env bash
set -e

echo "=== AetherHelm MCP Auto-Configuration (macOS / Linux) ==="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

MCP_EXE=""
CANDIDATES=(
  "$REPO_ROOT/build/aetherhelm-mcp"
  "$REPO_ROOT/build/Release/aetherhelm-mcp"
  "$REPO_ROOT/bin/aetherhelm-mcp"
  "$(which aetherhelm-mcp 2>/dev/null || true)"
)

for c in "${CANDIDATES[@]}"; do
  if [ -n "$c" ] && [ -x "$c" ]; then
    MCP_EXE="$c"
    break
  fi
done

if [ -z "$MCP_EXE" ]; then
  MCP_EXE="$REPO_ROOT/build/aetherhelm-mcp"
  echo "[!] Note: Executable not yet compiled at $MCP_EXE. Registering target path."
else
  echo "[+] Found AetherHelm MCP binary at: $MCP_EXE"
fi

update_json() {
  local path="$1"
  local name="$2"
  local dir
  dir="$(dirname "$path")"

  if [ ! -d "$dir" ]; then
    echo "[-] Skipping $name (Directory does not exist: $dir)"
    return
  fi

  # Use node or python to safely merge JSON without introducing dependencies if possible
  if command -v node >/dev/null 2>&1; then
    node -e "
      const fs = require('fs');
      let data = {};
      try { data = JSON.parse(fs.readFileSync('$path', 'utf8')); } catch(e) {}
      data.mcpServers = data.mcpServers || {};
      data.mcpServers.aetherhelm = { command: '$MCP_EXE', args: [] };
      fs.writeFileSync('$path', JSON.stringify(data, null, 2));
    "
    echo "[+] Configured $name -> $path"
  elif command -v python3 >/dev/null 2>&1; then
    python3 -c "
import json, os
p = '$path'
data = {}
if os.path.exists(p):
    try:
        with open(p, 'r') as f:
            data = json.load(f)
    except:
        pass
if 'mcpServers' not in data:
    data['mcpServers'] = {}
data['mcpServers']['aetherhelm'] = {'command': '$MCP_EXE', 'args': []}
with open(p, 'w') as f:
    json.dump(data, f, indent=2)
"
    echo "[+] Configured $name -> $path"
  else
    echo "[!] Neither node nor python3 available to modify $path safely."
  fi
}

# 1. Claude Desktop
if [[ "$OSTYPE" == "darwin"* ]]; then
  CLAUDE_PATH="$HOME/Library/Application Support/Claude/claude_desktop_config.json"
else
  CLAUDE_PATH="$HOME/.config/Claude/claude_desktop_config.json"
fi
update_json "$CLAUDE_PATH" "Claude Desktop"

# 2. Cursor
CURSOR_PATH="$HOME/.cursor/mcp.json"
update_json "$CURSOR_PATH" "Cursor"

# 3. Windsurf
WINDSURF_PATH="$HOME/.codeium/windsurf/mcp_config.json"
update_json "$WINDSURF_PATH" "Windsurf"

# 4. Cline / Roo-Code
CLINE_PATH="$HOME/.config/Code/User/globalStorage/saoudrizwan.claude-dev/settings/cline_mcp_settings.json"
update_json "$CLINE_PATH" "Cline"

ROO_PATH="$HOME/.config/Code/User/globalStorage/rooveterinaryinc.roo-cline/settings/cline_mcp_settings.json"
update_json "$ROO_PATH" "Roo-Code"

echo ""
echo "[✓] AetherHelm MCP Server configuration complete!"
