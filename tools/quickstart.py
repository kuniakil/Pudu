#!/usr/bin/env python3
# ----------------------------------------------------------------------
# 谱渡 Pudu · 一键出图（跨平台 quickstart）
#
# 一条命令跑完整链路，产物落在输出目录：
#   图片/PDF  →  Audiveris(OMR)  →  MusicXML  →  Pudu 内核  →  简谱
#   →  jianpu.html（可打印） + jianpu.png + jianpu.pdf
#
# 用法:
#   python3 tools/quickstart.py <输入.png|jpg|pdf> [--out 输出目录]
#                              [--engine audiveris|oemer|fixture]
#                              [--keep]      # 保留中间产物
#
# 依赖:
#   * Pudu 二进制（同目录 build/ 下，或用 env PUDU_EXE 指定）
#   * Audiveris（macOS: /Applications/Audiveris.app；Linux: /opt/audiveris）
#   * Chrome/Chromium（仅用于导出 PNG/PDF；缺失时只出 HTML，不报错）
# ----------------------------------------------------------------------
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)


def find_pudu() -> str:
    """定位 Pudu 二进制：env > build/ 各 preset 子目录 > build/ 根。"""
    env = os.environ.get("PUDU_EXE")
    if env and os.path.isfile(env):
        return env
    names = ["Pudu.exe", "Pudu"]
    cands = []
    for d in ("", "macos-arm64", "linux-x64", "windows-msvc-vcpkg/Debug",
              "windows-msvc-vcpkg/Release"):
        for n in names:
            cands.append(os.path.join(REPO, "build", d, n) if d
                         else os.path.join(REPO, "build", n))
    for c in cands:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    sys.stderr.write(
        "[错误] 找不到 Pudu 二进制。\n"
        "  · 从 CI 下载 artifact 后解压到 build/ 目录，或\n"
        "  · 设置环境变量 PUDU_EXE 指向可执行文件。\n")
    sys.exit(1)


def run(cmd, **kw):
    """跑子进程并把输出透传给用户。返回 True 表示成功。"""
    sys.stderr.write("$ " + " ".join(cmd) + "\n")
    return subprocess.call(cmd, **kw) == 0


def main() -> int:
    ap = argparse.ArgumentParser(description="谱渡 Pudu 一键出图")
    ap.add_argument("input", help="输入图片或 PDF")
    ap.add_argument("--out", default=None, help="输出目录（默认 <输入目录>/pudu_out）")
    ap.add_argument("--engine", default="audiveris",
                    choices=["audiveris", "oemer", "fixture"])
    ap.add_argument("--keep", action="store_true", help="保留中间产物")
    args = ap.parse_args()

    src = os.path.abspath(args.input)
    if not os.path.isfile(src):
        sys.stderr.write(f"[错误] 输入不存在: {src}\n")
        return 2

    out_dir = os.path.abspath(args.out) if args.out else \
        os.path.join(os.path.dirname(src), "pudu_out")
    os.makedirs(out_dir, exist_ok=True)

    base = os.path.splitext(os.path.basename(src))[0]
    musicxml = os.path.join(out_dir, base + ".musicxml")
    html = os.path.join(out_dir, base + "_jianpu.html")

    pudu = find_pudu()

    # ---- 阶段 1：OMR（图片/PDF → MusicXML）----
    sys.stderr.write("\n=== 阶段 1/3 · OMR 识别（引擎: %s）===\n" % args.engine)
    if args.engine == "fixture":
        # fixture 引擎：C++ 内置，无外部依赖（用于验证链路，不做真实识别）
        if not run([pudu, "--from-omr", src, "--omr-engine", "fixture",
                    "--to-musicxml", musicxml]):
            return 1
    else:
        # 走 Python 适配层（内部会调 Audiveris / oemer 子进程）
        adapter = os.path.join(HERE, "omr_audiveris.py"
                               if args.engine == "audiveris" else "omr_oemer.py")
        if not run([sys.executable, adapter, src, musicxml]):
            sys.stderr.write("[错误] OMR 识别失败\n")
            return 1

    # ---- 阶段 2：简谱渲染（MusicXML → L2 HTML）----
    sys.stderr.write("\n=== 阶段 2/3 · 简谱转换（MusicXML → 简谱）===\n")
    if not run([pudu, musicxml, "--to-jianpu-l2", html]):
        return 1

    # ---- 阶段 3：导出 PNG / PDF（可选，缺 Chrome 时跳过）----
    sys.stderr.write("\n=== 阶段 3/3 · 导出 PNG / PDF ===\n")
    png = os.path.join(out_dir, base + "_jianpu.png")
    pdf = os.path.join(out_dir, base + "_jianpu.pdf")
    renderer = os.path.join(HERE, "render_jianpu.py")
    for target in (png, pdf):
        if not run([sys.executable, renderer, html, target]):
            sys.stderr.write("[警告] 未能导出 %s（缺 Chrome？HTML 已生成）\n"
                             % target)

    sys.stderr.write("\n=== 完成 · 产物 ===\n")
    for f in (musicxml, html, png, pdf):
        if os.path.isfile(f):
            sys.stderr.write("  %s  (%d bytes)\n"
                             % (f, os.path.getsize(f)))
    sys.stderr.write("\n用浏览器打开简谱：open '%s'  /  xdg-open '%s'\n"
                     % (html, html))
    return 0


if __name__ == "__main__":
    sys.exit(main())
