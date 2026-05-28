#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Builds the AU and VST3 as a universal (arm64 + x86_64) Release on macOS.
# Outputs land in build/ToneSuite_artefacts/Release/{AU,VST3,Standalone}/ and,
# because the CMakeLists has COPY_PLUGIN_AFTER_BUILD TRUE, are also copied to:
#   ~/Library/Audio/Plug-Ins/Components/Tone Suite.component
#   ~/Library/Audio/Plug-Ins/VST3/Tone Suite.vst3
# ---------------------------------------------------------------------------
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "==> Configuring ($BUILD_DIR)"
cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "$BUILD_DIR" --config Release --parallel

AU_PATH="$HOME/Library/Audio/Plug-Ins/Components/Tone Suite.component"
if [[ -d "$AU_PATH" ]]; then
    echo
    echo "==> Installed AU: $AU_PATH"
    echo
    echo "==> Validating with auval (this is what Logic runs internally):"
    auval -v aufx Pteq Yorc || {
        echo
        echo "auval validation FAILED."
        echo "Check the output above for the specific error - common causes:"
        echo "  - PLUGIN_CODE / PLUGIN_MANUFACTURER_CODE collide with another"
        echo "    installed plugin. Edit CMakeLists.txt and pick different codes."
        echo "  - macOS quarantine attribute. Try:"
        echo "      xattr -cr \"$AU_PATH\""
        exit 1
    }
    echo
    echo "==> Success. Open Logic, it will rescan the AU and you should see"
    echo "    'Tone Suite' under YourCompany in the plugin browser."
else
    echo "Build succeeded but AU was not copied. Check Xcode build output."
fi
