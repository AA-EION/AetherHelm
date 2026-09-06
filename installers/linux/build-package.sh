#!/usr/bin/env bash
set -euo pipefail

echo "============================================================"
echo " Packaging AetherHelm Linux AppImage and Plugin Archives   "
echo "============================================================"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
DIST_DIR="${REPO_ROOT}/dist"
APP_DIR="${REPO_ROOT}/AppDir"

mkdir -p "${DIST_DIR}"
rm -rf "${APP_DIR}"
mkdir -p "${APP_DIR}/usr/bin"
mkdir -p "${APP_DIR}/usr/lib"
mkdir -p "${APP_DIR}/usr/share/applications"
mkdir -p "${APP_DIR}/usr/share/icons/hicolor/256x256/apps"

# 1. Locate Standalone Executable
STANDALONE_SRC=""
for path in \
    "${BUILD_DIR}/AetherHelm_artefacts/Release/Standalone/AetherHelm" \
    "${BUILD_DIR}/AetherHelm_artefacts/Standalone/AetherHelm"; do
    if [ -f "${path}" ]; then
        STANDALONE_SRC="${path}"
        break
    fi
done

if [ -n "${STANDALONE_SRC}" ]; then
    echo "[+] Found Standalone Binary: ${STANDALONE_SRC}"
    cp "${STANDALONE_SRC}" "${APP_DIR}/usr/bin/AetherHelm"
    chmod +x "${APP_DIR}/usr/bin/AetherHelm"
fi

# 2. Locate MCP Server Binary
MCP_SRC=""
for path in \
    "${BUILD_DIR}/aetherhelm-mcp" \
    "${BUILD_DIR}/aetherhelm-mcp_artefacts/Release/aetherhelm-mcp" \
    "${BUILD_DIR}/aetherhelm-mcp_artefacts/aetherhelm-mcp"; do
    if [ -f "${path}" ]; then
        MCP_SRC="${path}"
        break
    fi
done

if [ -n "${MCP_SRC}" ]; then
    echo "[+] Found MCP Server Binary: ${MCP_SRC}"
    cp "${MCP_SRC}" "${APP_DIR}/usr/bin/aetherhelm-mcp"
    chmod +x "${APP_DIR}/usr/bin/aetherhelm-mcp"
fi

# 3. Create Desktop Entry and Icon
cat << 'EOF' > "${APP_DIR}/usr/share/applications/aetherhelm.desktop"
[Desktop Entry]
Type=Application
Name=AetherHelm
GenericName=Polyphonic Synthesizer
Comment=Modernized Polyphonic Synthesizer with AI and MCP Integration
Exec=AetherHelm
Icon=aetherhelm
Categories=AudioVideo;Audio;Midi;Music;
Terminal=false
StartupNotify=true
EOF

cp "${APP_DIR}/usr/share/applications/aetherhelm.desktop" "${APP_DIR}/aetherhelm.desktop"
cp "${REPO_ROOT}/images/helm_icon_256_2x.png" "${APP_DIR}/usr/share/icons/hicolor/256x256/apps/aetherhelm.png"
cp "${REPO_ROOT}/images/helm_icon_256_2x.png" "${APP_DIR}/aetherhelm.png"
cp "${REPO_ROOT}/images/helm_icon_256_2x.png" "${APP_DIR}/.DirIcon"

# 4. AppRun entrypoint script
cat << 'EOF' > "${APP_DIR}/AppRun"
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
exec "${HERE}/usr/bin/AetherHelm" "$@"
EOF
chmod +x "${APP_DIR}/AppRun"

# 5. Build AppImage
ARCH=$(uname -m)
APPIMAGE_OUT="${DIST_DIR}/AetherHelm-${ARCH}.AppImage"

if ! command -v appimagetool >/dev/null 2>&1; then
    echo "[*] Downloading appimagetool..."
    curl -sSL -o /tmp/appimagetool "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${ARCH}.AppImage" || true
    chmod +x /tmp/appimagetool || true
    if [ -f /tmp/appimagetool ]; then
        /tmp/appimagetool --appimage-extract-and-run "${APP_DIR}" "${APPIMAGE_OUT}" 2>/dev/null || \
        /tmp/appimagetool --no-appstream "${APP_DIR}" "${APPIMAGE_OUT}" 2>/dev/null || true
    fi
else
    appimagetool --no-appstream "${APP_DIR}" "${APPIMAGE_OUT}" || true
fi

# Fallback to Tarball for Standalone AppDir if appimagetool is unavailable on minimal runners
if [ ! -f "${APPIMAGE_OUT}" ]; then
    echo "[!] AppImage generation skipped/failed; creating standalone AppDir archive..."
    tar -czf "${DIST_DIR}/AetherHelm-Standalone-${ARCH}.tar.gz" -C "${REPO_ROOT}" AppDir
fi

# 6. Build Standard Linux Plugins Archive (.vst3, .clap)
PLUGIN_STAGING="${REPO_ROOT}/plugin_staging"
rm -rf "${PLUGIN_STAGING}"
mkdir -p "${PLUGIN_STAGING}/vst3"
mkdir -p "${PLUGIN_STAGING}/clap"
mkdir -p "${PLUGIN_STAGING}/bin"
mkdir -p "${PLUGIN_STAGING}/scripts"

# Copy VST3
for path in \
    "${BUILD_DIR}/AetherHelm_artefacts/Release/VST3/AetherHelm.vst3" \
    "${BUILD_DIR}/AetherHelm_artefacts/VST3/AetherHelm.vst3"; do
    if [ -d "${path}" ]; then
        echo "[+] Found VST3: ${path}"
        cp -R "${path}" "${PLUGIN_STAGING}/vst3/"
        break
    fi
done

# Copy CLAP (if built)
for path in \
    "${BUILD_DIR}/AetherHelm_artefacts/Release/CLAP/AetherHelm.clap" \
    "${BUILD_DIR}/AetherHelm_artefacts/CLAP/AetherHelm.clap"; do
    if [ -d "${path}" ] || [ -f "${path}" ]; then
        echo "[+] Found CLAP: ${path}"
        cp -R "${path}" "${PLUGIN_STAGING}/clap/"
        break
    fi
done

# Copy MCP Binary and scripts
if [ -n "${MCP_SRC}" ]; then
    cp "${MCP_SRC}" "${PLUGIN_STAGING}/bin/aetherhelm-mcp"
    chmod +x "${PLUGIN_STAGING}/bin/aetherhelm-mcp"
fi
cp -r "${REPO_ROOT}/scripts/"* "${PLUGIN_STAGING}/scripts/"

# Create install-plugins.sh helper
cat << 'EOF' > "${PLUGIN_STAGING}/install-plugins.sh"
#!/bin/sh
set -e
echo "Installing AetherHelm plugins to user directories (~/.vst3, ~/.clap)..."
mkdir -p "${HOME}/.vst3" "${HOME}/.clap" "${HOME}/.local/bin"
if [ -d "vst3/AetherHelm.vst3" ]; then
    cp -r vst3/AetherHelm.vst3 "${HOME}/.vst3/"
    echo "[+] Installed VST3 to ${HOME}/.vst3/AetherHelm.vst3"
fi
if [ -e "clap/AetherHelm.clap" ]; then
    cp -r clap/AetherHelm.clap "${HOME}/.clap/"
    echo "[+] Installed CLAP to ${HOME}/.clap/AetherHelm.clap"
fi
if [ -f "bin/aetherhelm-mcp" ]; then
    cp bin/aetherhelm-mcp "${HOME}/.local/bin/"
    echo "[+] Installed MCP server to ${HOME}/.local/bin/aetherhelm-mcp"
fi
echo "[+] Done!"
EOF
chmod +x "${PLUGIN_STAGING}/install-plugins.sh"

tar -czf "${DIST_DIR}/AetherHelm-Linux-Plugins-${ARCH}.tar.gz" -C "${PLUGIN_STAGING}" .
echo "[+] Successfully created Linux Plugin archive: ${DIST_DIR}/AetherHelm-Linux-Plugins-${ARCH}.tar.gz"
