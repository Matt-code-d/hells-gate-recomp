




set -euo pipefail

HERE="${DANTES_HOME:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
PROTON="${PROTON:-}"

if [ -z "$PROTON" ]; then
  
  compat_dir="$HOME/.local/share/Steam/compatibilitytools.d"
  if [ -d "$compat_dir" ]; then
    PROTON=$(find "$compat_dir" -maxdepth 2 -iname proton -type f 2>/dev/null | sort -V | tail -1)
  fi
fi

if [ -z "$PROTON" ] || [ ! -x "$PROTON" ]; then
  echo "Proton not found. Set PROTON=/path/to/proton (a GE-Proton build works well)."
  exit 1
fi

[ -e "$HERE/dantes_inferno.exe" ] || { echo "dantes_inferno.exe not found in $HERE"; exit 1; }
[ -e "$HERE/game/default.xex" ] || { echo "game/default.xex missing under $HERE/game"; exit 1; }


export STEAM_COMPAT_DATA_PATH="$HERE/prefix"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_COMPAT_CLIENT_INSTALL_PATH:-$HOME/.local/share/Steam}"
mkdir -p "$STEAM_COMPAT_DATA_PATH"


if [ "${MANGOHUD:-0}" = 1 ]; then
  mkdir -p "$HERE/fpslog"
  export MANGOHUD=1
  export MANGOHUD_CONFIG="${MANGOHUD_CONFIG:-output_folder=$HERE/fpslog,autostart_log=1,log_duration=0,log_interval=100}"
fi










GAME_ARGS=(
  "--game_data_root=$HERE/game"
  "--draw_resolution_scale_x=2"
  "--draw_resolution_scale_y=2"
  "--anisotropic_override=5"
  "--native_2x_msaa=true"
  "--present_dither=true"
)


first_run=0
[ -d "$STEAM_COMPAT_DATA_PATH/pfx" ] || first_run=1



"$PROTON" run "$HERE/dantes_inferno.exe" "${GAME_ARGS[@]}" "$@" || true

if [ "$first_run" = 1 ]; then
  echo "Prefix created. Starting the game..."
  exec "$PROTON" run "$HERE/dantes_inferno.exe" "${GAME_ARGS[@]}" "$@"
fi
