# 谱渡 Pudu · macOS / Linux 移植说明

> 本文件描述本 fork（`kuniakil/Pudu`）相对上游 `youzi467/Pudu` 增加的跨平台支持。
> 上游 README 仍然是功能与架构的权威文档，本文件只讲 **macOS / Linux 的差异与操作**。

---

## 1. 这个 fork 做了什么

上游 Pudu 的 C++ 核心把三处 Windows 专属的操作系统调用直接写在业务代码里
（`CreateProcess` / `MultiByteToWideChar` / `GetEnvironmentVariable`），
在 macOS / Linux 上无法编译。本 fork 做了三件事：

| 改动 | 内容 |
|---|---|
| **平台层** | 新增 `include/platform/platform.hpp` + `src/platform/platform_win.cpp` / `platform_posix.cpp`，把子进程执行、环境变量、文件存在性、XML 路径、可执行文件定位收敛到统一接口。Windows 实现是上游原码的逐字移植，**行为零变化** |
| **Python 侧** | Audiveris 启动器路径按平台选址；Pudu 二进制名跨平台；单实例互斥补 POSIX（flock）实现；选程序对话框筛选按平台放宽 |
| **CI** | 新增 `.github/workflows/build.yml`：matrix 跑 `macos-15`（Apple Silicon）+ `ubuntu-24.04`，产出可下载的 artifact |

**转换逻辑一行未改** —— 简谱渲染、变调、后处理规则引擎、MusicXML 序列化
全部沿用上游实现，因此识别质量与上游 Windows 版一致。

---

## 2. 前置依赖

### 2.1 Audiveris（OMR 识别引擎，必需）

Audiveris 是 Java 应用，官方为三平台分别提供安装包，各自带 JRE（无需另装 Java）。

**macOS（Apple Silicon / Intel）**

```bash
# 下载（约 81MB）：https://github.com/Audiveris/audiveris/releases/tag/5.11.0
#   Audiveris-5.11.0-macosx-arm64.dmg     ← M1/M2/M3/M4
#   Audiveris-5.11.0-macosx-x86_64.dmg    ← Intel
hdiutil attach Audiveris-5.11.0-macosx-arm64.dmg
cp -R /Volumes/Audiveris/Audiveris.app /Applications/
hdiutil detach /Volumes/Audiveris
```

程序会自动找 `/Applications/Audiveris.app/Contents/MacOS/Audiveris`，
**无需设置任何环境变量**。

> ⚠️ Audiveris 官方 dmg 未做代码签名。若从浏览器下载，macOS Gatekeeper 会拦截，
> 需在「系统设置 → 隐私与安全性」点「仍要打开」；或直接用 `curl` 下载
> （curl 不会打 quarantine 标记，可绕过此提示）。

**Linux**

```bash
# https://github.com/Audiveris/audiveris/releases/tag/5.11.0
sudo apt install ./Audiveris-5.11.0-ubuntu24.04-x86_64.deb
# 安装到 /opt/audiveris/bin/Audiveris，程序自动识别
```

> Audiveris 的 Linux 发行版**只有 x86_64**。aarch64 机器（如 Raspberry Pi /
> ARM 服务器）无法使用官方 deb；此 fork 未包含 aarch64 方案。
> ARM 机器请改用 `--omr-engine oemer`（纯 PyTorch，跨平台），或自行用
> `java -jar audiveris.jar` 方式适配。

**覆盖默认路径**（任意平台）：

```bash
export PUDU_AUDIVERIS_EXE=/path/to/Audiveris
```

### 2.2 Chrome / Chromium（仅导出 PNG / PDF 时需要）

简谱 L2 的主输出是自包含 HTML，本身可直接在浏览器打开并打印。
若需要 PNG / PDF 文件，用 headless Chrome 渲染（见 `tools/render_jianpu.py`）。

---

## 3. 构建

### 3.1 GitHub Actions（推荐，无需本地编译）

推送到本 fork 的任意分支即触发：

```
https://github.com/kuniakil/Pudu/actions
```

matrix：`macos-15`（Apple Silicon）+ `ubuntu-24.04`。
每次跑完可下载 artifact（含二进制与 `tools/` 目录）。

下载后解压，目录结构建议：

```
Pudu/
├── Pudu            ← 二进制（macOS 无 .exe 后缀）
└── tools/          ← Python 引擎适配层
```

> 二进制会用「同级的 `tools/`」定位 OMR 适配层，所以两者放一起即可用。
> 若要放在别处，设 `PUDU_TOOLS_DIR` 指向 `tools/` 的绝对路径。

### 3.2 本地构建

```bash
# macOS
brew install cmake ninja pugixml     # pugixml 可选，缺则自动 FetchContent 源码拉取
cmake --preset macos-arm64
cmake --build build/macos-arm64

# Linux
sudo apt install cmake ninja-build libpugixml-dev
cmake --preset linux-x64
cmake --build build/linux-x64

# 测试
ctest --test-dir build/macos-arm64 --output-on-failure
```

`pugixml` 的查找顺序：**已安装（vcpkg / brew / apt）→ FetchContent 源码回退**，
所以没有 vcpkg 也能构建。

---

## 4. 使用

### 4.1 一键出图（最快看到效果）

```bash
python3 tools/quickstart.py 我的谱子.png
```

产物落在 `我的谱子.png` 同级的 `pudu_out/`：

```
pudu_out/
├── 我的谱子.musicxml       ← OMR 识别结果（music21 / MuseScore 可读）
├── 我的谱子_jianpu.html    ← 简谱（自包含，可直接打印）
├── 我的谱子_jianpu.png     ← 简谱图片
└── 我的谱子_jianpu.pdf     ← 简谱 PDF
```

参数：`--out <目录>` 指定输出目录、`--engine audiveris|oemer|fixture` 换引擎、
`--keep` 保留中间产物。

### 4.2 浏览器界面

```bash
python3 tools/pudu_server.py
# 浏览器打开提示的 http://127.0.0.1:8765
```

功能与上游一致：上传乐谱图片 / PDF → 简谱 L2 预览 → 下载 `final.musicxml` → 需校对面板。
右上角可切「oemer 回退」或「演示模式」（fixture，秒级、零依赖）。

### 4.3 桌面 App（原生窗口）

```bash
pip install pywebview
python3 tools/desktop_main.py
```

### 4.4 命令行

```bash
# 图片/PDF → 简谱 HTML
./Pudu --from-omr 谱子.png --to-jianpu-l2 out.html

# 显式指定引擎与 Audiveris 路径
./Pudu --from-omr 谱子.pdf --omr-engine audiveris --to-jianpu-l2 out.html

# MusicXML → 简谱
./Pudu score.musicxml --to-jianpu-l2 out.html

# 简谱 → MusicXML（反向）
./Pudu --from-jianpu-text jianpu.txt --to-musicxml out.musicxml

# HTML → PNG / PDF
python3 tools/render_jianpu.py out.html out.png
python3 tools/render_jianpu.py out.html out.pdf
```

---

## 5. 与上游的已知差异

| 项目 | 上游（Windows） | 本 fork |
|---|---|---|
| 编译 | MSVC + vcpkg（强制） | MSVC + vcpkg / Ninja + 系统 pugixml / FetchContent 三选一 |
| OMR 引擎 | Audiveris.exe（Windows） | 同 CLI 参数，路径按平台选址 |
| 单实例互斥 | Windows 具名互斥体 | Windows 具名互斥体 / POSIX flock |
| 打包分发 | PyInstaller + Inno Setup（Windows） | 未实现（当前只出 tar.gz artifact） |
| 桌面包 | pywebview（Windows） | pywebview 跨平台可用，筛选器按平台放宽 |

**转换质量不因平台而异** —— 核心转换逻辑完全共用，差异只在 OMR 引擎的调用方式。

---

## 6. 许可

- 本 fork 的代码：沿用上游 MIT 许可（见 `LICENSE`），版权归原作者 朱禹泽。
- Audiveris：**AGPL-3.0**。本项目以**独立子进程**方式调用它，不链接、不内嵌其代码，
  故不触发 AGPL 的传染性条款；若将来把 Audiveris **打包进发行产物**，
  需重新评估 AGPL 义务（上游 `packaging/AV_LICENSE.txt` 已就此留有说明）。
