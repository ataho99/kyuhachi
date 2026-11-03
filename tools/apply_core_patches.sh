#!/bin/bash
# S2: DOSBox-X 원본(DOSBOX_X.lock 의 sha256 과 일치해야 함)에 core_patches/*.patch 를 적용해 컴파일용 사본을 만든다.
# 사용: tools/apply_core_patches.sh <출력폴더>   (환경변수 KH_DBX_SRC = 원본 src 폴더, 기본 /mnt/y/dbx_ws/ana/src/src)
# 출력: bios_B.cpp (= ints/bios.cpp + 패치) · mixer_P.cpp (= hardware/mixer.cpp + 패치). 나머지 패치는 --check 만(미리 빌드한 obj 가 쓰는 파일).
R=$(cd "$(dirname "$0")/.." && pwd); LOCK=$R/DOSBOX_X.lock; OUT=${1:?출력폴더}; ORIG=${KH_DBX_SRC:-/mnt/y/dbx_ws/ana/src/src}
[ -f "$LOCK" ] || { echo "apply_core: DOSBOX_X.lock 없음"; exit 2; }
T=$(mktemp -d) || exit 2; trap 'rm -rf "$T"' EXIT
bad=0; n=0
while read -r sha rel; do case "$sha" in \#*|"") continue;; esac
  n=$((n+1)); f="$ORIG/$rel"; [ -f "$f" ] || { echo "apply_core: 원본 없음 $rel"; bad=1; continue; }
  got=$(sha256sum "$f" | cut -c1-64); [ "$got" = "$sha" ] || { echo "apply_core: sha 불일치 $rel (lock ${sha:0:12} / 실제 ${got:0:12})"; bad=1; continue; }
  mkdir -p "$T/$(dirname "$rel")"; cp "$f" "$T/$rel"; done < "$LOCK"
[ $bad = 0 ] || exit 3
cd "$T" || exit 2
np=0; for p in "$R"/core_patches/*.patch; do [ -f "$p" ] || continue; np=$((np+1))
  git apply --check -p1 --whitespace=nowarn "$p" 2>&1 || { echo "apply_core: --check 실패 $(basename "$p")"; exit 4; }; done
for p in "$R"/core_patches/*.patch; do [ -f "$p" ] || continue; git apply -p1 --whitespace=nowarn "$p" || { echo "apply_core: 적용 실패 $(basename "$p")"; exit 4; }; done
mkdir -p "$OUT" && cp "$T/ints/bios.cpp" "$OUT/bios_B.cpp" && cp "$T/hardware/mixer.cpp" "$OUT/mixer_P.cpp" || exit 5
echo "apply_core: 원본 $n개 sha 일치 · 패치 $np개 --check/적용 OK · bios_B.cpp $(sha256sum "$OUT/bios_B.cpp" | cut -c1-12) mixer_P.cpp $(sha256sum "$OUT/mixer_P.cpp" | cut -c1-12)"
