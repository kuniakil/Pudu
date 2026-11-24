#!/usr/bin/env bash
# ----------------------------------------------------------------------
# 抓取《月下煮茶》ground truth（五线谱 + 简谱）
#
# 来源：everyonepiano.cn（eopiano.com 中文站）
#   页面  https://everyonepiano.cn/Piano-19895.html
#   简谱  3 张 = 用户 Nextcloud 上的 月下煮茶01~03.png（byte-for-byte 相同）
#   五线谱 5 张 = 同一部作品的五线谱版（本项目要 OMR 的输入）
#
# 用法:
#   bash tools/fetch_ground_truth.sh [输出目录]
#   默认输出目录 = data/yxdc
#
# 幂等：已存在且 sha256 吻合的文件会跳过。
# ----------------------------------------------------------------------
set -euo pipefail

OUT="${1:-$(cd "$(dirname "$0")/.." && pwd)/data/yxdc}"
BASE="https://everyonepiano.cn/pianomusic/020/0019895/0019895"

mkdir -p "$OUT"
cd "$OUT"

# ---- 显式下载（序号写死，可读性优先；每个文件校验 sha256）----
fetch() {  # fetch <序号> <w|j> <目标文件> <期望sha256>
  local n="$1" k="$2" file="$3" want="$4"
  if [ -f "$file" ] && [ "$(shasum -a 256 "$file" | cut -d' ' -f1)" = "$want" ]; then
    echo "  [skip] $file"
    return 0
  fi
  echo "  [get ] $file  <- ${BASE}-${k}-b-${n}.png"
  curl -sS -L -A "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)" \
       -o "$file" "${BASE}-${k}-b-${n}.png"
  local got
  got="$(shasum -a 256 "$file" | cut -d' ' -f1)"
  if [ "$got" != "$want" ]; then
    echo "  [FAIL] $file sha256 不符" >&2
    echo "         期望 $want" >&2
    echo "         实际 $got" >&2
    return 1
  fi
  echo "  [ok  ] $file  sha256 吻合"
}

echo "==> 抓取到 $OUT"
fetch 1 w yxdc-staff-1.png 5d021c2f7f85fff37e39a01896d9f9d59d8a2448b7f39eb07908d3e2ec340b2b
fetch 2 w yxdc-staff-2.png 5a0ea04ea2c0551ac8578309ea2325fd5cd99b4aafd944cdcffbab848f2aa0da
fetch 3 w yxdc-staff-3.png 45b35b6db31afeaca28dbd18818d9e6271e453070717e6951c05f7de912294f7
fetch 4 w yxdc-staff-4.png 4912564e59b4d490c3849364b5861cf2dff9143062db372dd0aabba0fa5722cf
fetch 5 w yxdc-staff-5.png a0d96936b4b1dde60a47c58383289438231cbf11fc653312314117280009e74b
fetch 1 j yxdc-jianpu-1.png 319def0ab03060590e9d183976ec9490bd8c5060ecb5a023fe49db38bab84423
fetch 2 j yxdc-jianpu-2.png da330cb0ebc6f792cc843fb013efe03e82fcd5516d44131c35e4f8f804229d28
fetch 3 j yxdc-jianpu-3.png 193de1c2855c51f5a81b0a2bab8e4a6a9e45a9390fc9802422509c9394ce6840

echo "==> 完成，8 个文件全部通过 sha256 校验"
echo "    详见 $OUT/MANIFEST.md"
