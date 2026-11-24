#!/usr/bin/env python3
# ----------------------------------------------------------------------
# 谱渡 Pudu · 简谱 L2 HTML → PNG / PDF 渲染器
#
# 上游的 L2 输出是自包含 HTML（含真实八度点、减时线连写、增时线、和弦列、
# 连音弧），本身「可打印」。本脚本用 headless Chrome 把它落成图片或 PDF，
# 便于分享、存档、贴进笔记。
#
# 用法:
#   python3 tools/render_jianpu.py <input.html> <output.png|output.pdf>
#   python3 tools/render_jianpu.py in.html out.png --width 1800
#
# 浏览器选址：env PUDU_CHROME → 平台常见安装路径 → PATH。
# 依赖：Google Chrome / Chromium / Edge 任一（无则报错并给出安装提示）。
# ----------------------------------------------------------------------
import os
import shutil
import subprocess
import sys
import tempfile
from typing import List, Optional

# 常见安装路径（按优先级）
_CHROME_CANDIDATES = [
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    "/Applications/Chromium.app/Contents/MacOS/Chromium",
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
    "/usr/bin/google-chrome",
    "/usr/bin/google-chrome-stable",
    "/usr/bin/chromium",
    "/usr/bin/chromium-browser",
    "/snap/bin/chromium",
]
_PATH_NAMES = ["google-chrome", "google-chrome-stable", "chromium",
               "chromium-browser", "microsoft-edge"]


def resolve_chrome() -> Optional[str]:
    """选址：env PUDU_CHROME → 平台常见路径 → PATH。"""
    env = os.environ.get("PUDU_CHROME")
    if env:
        return env if os.path.isfile(env) else None
    for c in _CHROME_CANDIDATES:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    for name in _PATH_NAMES:
        found = shutil.which(name)
        if found:
            return found
    return None


def render(html_path: str, out_path: str, width: int = 1800,
           timeout: int = 120) -> int:
    """把 HTML 渲染成 PNG 或 PDF（按 out_path 后缀决定）。返回进程退出码。"""
    chrome = resolve_chrome()
    if not chrome:
        sys.stderr.write(
            "[错误] 未找到 Chrome/Chromium/Edge。请安装 Google Chrome，"
            "或设置环境变量 PUDU_CHROME 指向可执行文件。\n")
        return 1

    if not os.path.isfile(html_path):
        sys.stderr.write(f"[错误] 输入 HTML 不存在: {html_path}\n")
        return 1

    ext = os.path.splitext(out_path)[1].lower()
    url = "file://" + os.path.abspath(html_path)

    # headless 在较新 Chrome 里是 --headless=new；旧版用 --headless。
    # 两种都试一遍，取先成功的。
    headless_flags = [["--headless=new"], ["--headless"]]

    with tempfile.TemporaryDirectory(prefix="pudu_render_") as tmp:
        profile = os.path.join(tmp, "profile")
        for headless in headless_flags:
            if ext == ".pdf":
                cmd = [chrome] + headless + [
                    "--no-pdf-header-footer",
                    "--print-to-pdf=" + os.path.abspath(out_path),
                    "--user-data-dir=" + profile,
                    "--virtual-time-budget=10000",
                    url,
                ]
            else:
                cmd = [chrome] + headless + [
                    "--screenshot=" + os.path.abspath(out_path),
                    "--window-size=%d,%d" % (width, 1200),
                    "--hide-scrollbars",
                    "--force-device-scale-factor=2",   # 2x 缩放，图片更清晰
                    "--user-data-dir=" + profile,
                    "--virtual-time-budget=10000",
                    url,
                ]
            try:
                proc = subprocess.run(cmd, capture_output=True, timeout=timeout)
            except subprocess.TimeoutExpired:
                sys.stderr.write("[错误] 渲染超时\n")
                return 1
            if proc.returncode == 0 and os.path.isfile(out_path) and \
                    os.path.getsize(out_path) > 0:
                sys.stdout.write(f"[ok] 已渲染 {out_path} "
                                 f"({os.path.getsize(out_path)} bytes)\n")
                return 0
            last_err = (proc.stderr or b"").decode("utf-8", errors="replace")[-800:]
            if proc.returncode != 0 and last_err:
                sys.stderr.write(f"[警告] {' '.join(headless)} 渲染失败: {last_err}\n")

    sys.stderr.write("[错误] 渲染失败（两种 headless 模式均未产出文件）\n")
    return 1


def main(argv: List[str]) -> int:
    if len(argv) < 2:
        sys.stderr.write("用法: python3 tools/render_jianpu.py "
                         "<input.html> <output.png|output.pdf> [--width N]\n")
        return 2
    html_path, out_path = argv[0], argv[1]
    width = 1800
    if "--width" in argv:
        i = argv.index("--width")
        if i + 1 < len(argv):
            width = int(argv[i + 1])
    return render(html_path, out_path, width)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
