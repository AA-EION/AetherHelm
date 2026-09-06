#!/usr/bin/env bash
set -euo pipefail

echo "============================================================"
echo " Packaging AetherHelm macOS Universal .pkg with Ad-Hoc Sign "
echo "============================================================"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
DIST_DIR="${REPO_ROOT}/dist"
PKG_ROOT="${REPO_ROOT}/pkg_root"

rm -rf "${PKG_ROOT}"
mkdir -p "${PKG_ROOT}/Applications"
mkdir -p "${PKG_ROOT}/Library/Audio/Plug-Ins/Components"
mkdir -p "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3"
mkdir -p "${PKG_ROOT}/Library/Audio/Plug-Ins/CLAP"
mkdir -p "${PKG_ROOT}/usr/local/bin"
mkdir -p "${DIST_DIR}"

# 1. Locate and Copy Standalone App
STANDALONE_SRC=""
for path in \
    "${BUILD_DIR}/AetherHelm_artefacts/Release/Standalone/AetherHelm.app" \
    "${BUILD_DIR}/AetherHelm_artefacts/Standalone/AetherHelm.app"; do
    if [ -d "${path}" ]; then
        STANDALONE_SRC="${path}"
        break
    fi
done

if [ -n "${STANDALONE_SRC}" ]; then
    echo "[+] Found Standalone App: ${STANDALONE_SRC}"
    cp -R "${STANDALONE_SRC}" "${PKG_ROOT}/Applications/"
fi

# 2. Locate and Copy VST3 Plugin
VST3_SRC=""
for path in \
    "${BUILD_DIR}/AetherHelm_artefacts/Release/VST3/AetherHelm.vst3" \
    "${BUILD_DIR}/AetherHelm_artefacts/VST3/AetherHelm.vst3"; do
    if [ -d "${path}" ]; then
        VST3_SRC="${path}"
        break
    fi
done

if [ -n "${VST3_SRC}" ]; then
    echo "[+] Found VST3 Plugin: ${VST3_SRC}"
    cp -R "${VST3_SRC}" "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3/"
fi

# 3. Locate and Copy AU (Component) Plugin
AU_SRC=""
for path in \
    "${BUILD_DIR}/AetherHelm_artefacts/Release/AU/AetherHelm.component" \
    "${BUILD_DIR}/AetherHelm_artefacts/AU/AetherHelm.component"; do
    if [ -d "${path}" ]; then
        AU_SRC="${path}"
        break
    fi
done

if [ -n "${AU_SRC}" ]; then
    echo "[+] Found AU Plugin: ${AU_SRC}"
    cp -R "${AU_SRC}" "${PKG_ROOT}/Library/Audio/Plug-Ins/Components/"
fi

# 4. Locate and Copy CLAP Plugin (if built)
for path in \
    "${BUILD_DIR}/AetherHelm_artefacts/Release/CLAP/AetherHelm.clap" \
    "${BUILD_DIR}/AetherHelm_artefacts/CLAP/AetherHelm.clap"; do
    if [ -d "${path}" ] || [ -f "${path}" ]; then
        echo "[+] Found CLAP Plugin: ${path}"
        cp -R "${path}" "${PKG_ROOT}/Library/Audio/Plug-Ins/CLAP/"
        break
    fi
done

# 5. Locate and Copy MCP Server Binary
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
    echo "[+] Found MCP Server binary: ${MCP_SRC}"
    cp "${MCP_SRC}" "${PKG_ROOT}/usr/local/bin/aetherhelm-mcp"
    chmod +x "${PKG_ROOT}/usr/local/bin/aetherhelm-mcp"
fi

# 6. Apply Ad-Hoc Code Signing to all bundles and binaries
echo "[*] Applying ad-hoc code signature (codesign --force --deep -s -)..."
find "${PKG_ROOT}" -name "*.app" -exec codesign --force --deep -s - {} + 2>/dev/null || true
find "${PKG_ROOT}" -name "*.vst3" -exec codesign --force --deep -s - {} + 2>/dev/null || true
find "${PKG_ROOT}" -name "*.component" -exec codesign --force --deep -s - {} + 2>/dev/null || true
find "${PKG_ROOT}" -name "*.clap" -exec codesign --force --deep -s - {} + 2>/dev/null || true
if [ -f "${PKG_ROOT}/usr/local/bin/aetherhelm-mcp" ]; then
    codesign --force -s - "${PKG_ROOT}/usr/local/bin/aetherhelm-mcp" 2>/dev/null || true
fi

# Clean up empty directories if certain formats were skipped
find "${PKG_ROOT}" -type d -empty -delete 2>/dev/null || true

# 7. Build Flat Installer Package (.pkg)
PKG_OUT="${DIST_DIR}/AetherHelm-macOS-Universal.pkg"
echo "[*] Building installer package at ${PKG_OUT}..."
pkgbuild \
    --root "${PKG_ROOT}" \
    --identifier "com.aeeion.aetherhelm.pkg" \
    --version "1.0.0" \
    --install-location "/" \
    "${PKG_OUT}"

echo "[+] Successfully built ${PKG_OUT}"
