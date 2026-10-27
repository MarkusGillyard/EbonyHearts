#!/usr/bin/env bash
# Shared paths and memory-capped wrappers for engine commands on FLOWBOX.
# Every heavy job runs in its own systemd scope so an out-of-memory kill takes the job, not the desktop.
UE="/media/cinqic/Cinqic Storage/UnrealEngine/UE_5.8.4"
PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
P="$PROJ_DIR/MortalSlopbat.uproject"
BUILDS="/media/cinqic/Cinqic Storage/UnrealEngine/Builds/MortalSlopbat"
capped() { systemd-run --user --scope -p MemoryMax="${MEM_MAX:-11G}" -p MemorySwapMax=512M --quiet "$@"; }
build_editor() { capped "$UE/Engine/Build/BatchFiles/Linux/Build.sh" MortalSlopbatEditor Linux Development "$P" -waitmutex -MaxParallelActions="${JOBS:-8}"; }
build_game() { capped "$UE/Engine/Build/BatchFiles/Linux/Build.sh" MortalSlopbat Linux "${1:-Development}" "$P" -waitmutex -MaxParallelActions="${JOBS:-8}"; }
editor_py() { capped "$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$P" -ExecutePythonScript="$1" -nullrhi -unattended -nosplash -nopause -stdout -FullStdOutLogOutput "${@:2}"; }
# Build, cook, stage and package for Linux. $1: Development or Shipping. Output goes to $BUILDS/<config>.
package_linux() {
	local cfg="${1:-Development}"; local extra=()
	[ "$cfg" = Shipping ] && extra+=(-nodebuginfo)
	capped "$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$P" -platform=Linux -clientconfig="$cfg" \
		-target=MortalSlopbat -installed -nop4 -utf8output -nocompileeditor -skipbuildeditor -unrealexe=UnrealEditor-Cmd \
		-build -cook -stage -pak -iostore -compressed -package -archive -unattended "${extra[@]}" \
		-archivedirectory="$BUILDS/$cfg"
}
# Runs automation tests whose names start with $1 (default "Slop") headless; prints a summary.
run_tests() {
	local filter="${1:-Slop}"; local log="$PROJ_DIR/Saved/Logs/tests.log"
	mkdir -p "$PROJ_DIR/Saved/Logs"
	capped "$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$P" -nullrhi -unattended -nopause -nosplash -NoSound \
		-ExecCmds="Automation RunTests $filter; Quit" -TestExit="Automation Test Queue Empty" -log -stdout > "$log" 2>&1
	grep -E "Test Completed\. Result=\{(Success|Fail)" "$log" | sed -E 's/.*Result=\{([A-Za-z]+)\} Name=\{([^}]*)\}.*/\1 \2/' | sort | uniq -c | sort -rn | awk '{print}' > "$log.summary"
	echo "passed: $(grep -c "Result={Success}" "$log")  failed: $(grep -c "Result={Fail}" "$log")"
	grep -E "Result=\{Fail" "$log" | sed -E 's/.*Name=\{([^}]*)\}.*/FAIL \1/'
	grep -E "^.*LogAutomationController: Error" "$log" | sed -E 's/.*Error: //' | head -40
}
# Runs the game from the editor build in a window. Extra arguments pass through (e.g. -SlopMatch=...).
# With no monitor attached (display switched off) a window's Vulkan presentation blocks forever;
# render offscreen instead (screenshots and timings still come from the GPU).
offscreen_flag() {
	if [ "$(xrandr --listmonitors 2>/dev/null | head -1 | awk '{print $2}')" = "0" ]; then echo -RenderOffscreen -ForceRes; fi
}

run_game() {
	capped "$UE/Engine/Binaries/Linux/UnrealEditor" "$P" -game -windowed -resx="${RESX:-1920}" -resy="${RESY:-1080}" -log -NoSplash -nosound=false $(offscreen_flag) "$@"
}
