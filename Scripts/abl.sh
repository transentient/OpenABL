# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026-present Open Handheld Collective
#   https://github.com/OpenHandheldCollective

# Builds and signs the ABL for one chipset. Sourced in the build container,
# from the ABL's tree:
#
#   build CHIPSET MENU SIGN MAKE-PARAM...
#
# Each chipset keeps its own build tree, moved into place while it builds
# because the generated makefiles contain absolute paths. The tree is
# rebuilt from scratch when the make parameters change, with ABL_CLEAN=1, or
# when an incremental build fails. The signed image goes to /out.

build() {
  soc=$1; menu=$2; sign=$3; shift 3
  echo "------------------------------------------------"
  echo "Starting build for Chipset: $soc..."
  B=obj/ABL_OBJ/Build/RELEASE_CLANG35
  if [ -d "$B" ]; then
    owner=$(cat "$B/.chipset" 2>/dev/null)
    if [ -n "$owner" ] && [ "$owner" != "$soc" ]; then rm -rf "$B.$owner"; mv "$B" "$B.$owner"
    elif [ -z "$owner" ]; then rm -rf "$B"; fi
  fi
  if [ ! -d "$B" ] && [ -d "$B.$soc" ]; then mv "$B.$soc" "$B"; fi
  rm -f unsigned_abl.elf
  # Use Conf from BaseTools' templates, not the Conf in Qualcomm's tree
  # (which links with lld through FUSE_LD).
  if ! grep -q "Wno-enum-compare" BaseTools/Conf/tools_def.template 2>/dev/null; then
    sed -i 's/-Wno-unused-but-set-variable/-Wno-unused-but-set-variable -Wno-unused-but-set-global -Wno-enum-compare -Wno-error/g' BaseTools/Conf/tools_def.template
  fi
  sed -i "s/RespDict\[Key\] = Value.replace('\\\\\\\\', '\/')/RespDict[Key] = Value/g" BaseTools/Source/Python/AutoGen/GenMake.py
  sed -i 's/gCommandMaxLength = 4096/gCommandMaxLength = 65536/g' BaseTools/Source/Python/Common/GlobalData.py
  mkdir -p BaseTools/Source/Python/distutils
  cat > BaseTools/Source/Python/distutils/__init__.py << 'PYEOF'
PYEOF
  cat > BaseTools/Source/Python/distutils/util.py << 'PYEOF'
import shlex
def split_quoted(s):
    return shlex.split(s)
PYEOF
  export PYTHONPATH="$PWD/BaseTools/Source/Python:${PYTHONPATH:-}"
  for c in build_rule tools_def target; do
    cmp -s "BaseTools/Conf/$c.template" "Conf/$c.txt" || { . edksetup.sh --reconfig; break; }
  done
  mkdir -p QcomModulePkg/Prebuilt/OpenAblMenu
  cp "$menu" QcomModulePkg/Prebuilt/OpenAblMenu/openabl-menu.efi
  if [ "$ABL_CLEAN" = 1 ] || [ "$(cat "$B/.make-args" 2>/dev/null)" != "$*" ]; then rm -rf "$B"; fi
  incremental=0; [ -d "$B" ] && incremental=1
  mkdir -p "$B"; echo "$soc" > "$B/.chipset"; echo "$*" > "$B/.make-args"
  if ! make all "$@"; then
    [ "$incremental" = 1 ] || { echo "ERROR: Build failed for $soc."; return 1; }
    echo "  > Incremental build failed; building $soc from scratch"
    rm -rf "$B" unsigned_abl.elf; mkdir -p "$B"
    echo "$soc" > "$B/.chipset"; echo "$*" > "$B/.make-args"
    make all "$@" || { echo "ERROR: Build failed for $soc."; return 1; }
  fi
  out=abl_signed-$(echo "$soc" | tr a-z A-Z).elf
  qtestsign_dir="${QTESTSIGN_DIR:-/qtestsign}"
  out_dir="${OUT_DIR:-/out}"
  python3 "$qtestsign_dir/qtestsign.py" "$sign" -o "$out_dir/$out" abl unsigned_abl.elf || { echo "ERROR: Signing failed for $soc."; return 1; }
  (cd "$out_dir" && sha256sum "$out" > "$out.sha256")
  echo "SUCCESS: $out"
}
