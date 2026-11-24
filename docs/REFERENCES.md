# 参考资料与外部资源索引

> 本文件汇总「去哪里找什么」——语料位置、抓取规律、现成服务、候选开源项目。
> 分析与结论见 [`JIANPU-CONVERSION-NOTES.md`](JIANPU-CONVERSION-NOTES.md)，移植说明见
> [`PORT-macos-linux.md`](PORT-macos-linux.md)。

---

## 0. 文档地图（下次接手先读这张表）

| 你想知道 | 看哪一节 |
|---|---|
| 上次做到哪、下一步做什么 | `JIANPU-CONVERSION-NOTES.md` §9 行动清单 |
| 目标格式和 Pudu 差在哪 | `JIANPU-CONVERSION-NOTES.md` §3 |
| 难点到底难在哪 | `JIANPU-CONVERSION-NOTES.md` §4 |
| 歌词为什么是死墙 | `JIANPU-CONVERSION-NOTES.md` §5 |
| 有哪些路可以走 | `JIANPU-CONVERSION-NOTES.md` §7 |
| 参考语料在哪 | **本文** §1 |
| 怎么拿到「五线谱 + 对应简谱」的配对 | **本文** §2 ⭐ |
| 有没有现成服务能做 | **本文** §3 |
| 要 fork 别人哪些仓库 | **本文** §4 |
| OMR 引擎怎么装 | **本文** §5 |

---

## 1. 参考语料

### 1.1 位置

Nextcloud 实体在 **n100**（ssh `n100`，用户 `kuniaki`），路径 `~/文件/曲譜/`。

> 本机（Mac）**没有挂载**（`/Users/mlee/mnt` 是空的），要用：
> ```bash
> scp n100:'~/文件/曲譜/*' /tmp/jianpu_ref/
> ```

### 1.2 10 个文件与它们的体裁归属

| 文件 | 体裁 | 说明 |
|---|---|---|
| `如願 好聽版 01~04.png` | **双手简谱** ⭐ | eopiano.cn 成品扫描件，2381×3368 PNG。左右手都是数字、**无和弦标记**、有歌词 |
| `月下煮茶01~03.png` | **双手简谱** ⭐ | 同上，纯器乐（**无歌词**） |
| `如願.jpg` / `如願01.jpg` / `如願02.jpg` | **伴奏谱**（数字谱） | 数字+节奏线+歌词+**和弦标记**（Bm7 / E7 / Amaj7 / Dmaj7 / G#m7(b5) / C#7 …） |

> ⚠️ 那 3 个 JPG **不是五线谱**，Audiveris 对它们完全无效。
> **本项目的目标是「双手简谱」**（PNG 那批），不是「伴奏谱」（JPG 那批）。

---

## 2. ⭐ ground truth 配对（最有价值的一条）

**everyonepiano.cn（= eopiano.com 中文站）同一页面同时提供五线谱与简谱。**

《月下煮茶》页面 `https://everyonepiano.cn/Piano-19895.html`：**5 张五线谱 + 3 张简谱**，
而那 3 张简谱**正是** §1.1 里的 `月下煮茶01~03.png`。

图片 URL 规律（已实测）：

```
https://everyonepiano.cn/pianomusic/020/0019895/0019895-w-b-{N}.png   ← 五线谱 N=1..5
https://everyonepiano.cn/pianomusic/020/0019895/0019895-j-b-{N}.png   ← 简谱   N=1..3
```

**抓取注意**：`www.everyonepiano.cn` 的 DNS 解析不了，**去掉 `www.`** 才能访问；
但图片 URL 带不带 `www.` 都能取到。

```bash
mkdir -p /tmp/yxdc && cd /tmp/yxdc
for n in 1 2 3 4 5; do
  for k in w j; do
    curl -sS -L -A "Mozilla/5.0" -o "${k}${n}.png" \
      "https://everyonepiano.cn/pianomusic/020/0019895/0019895-${k}-b-${n}.png"
  done
done
```

**为什么重要**：这是同一部作品的「输入 → 期望输出」配对，能**逐小节量化**整条链路的准确率，
把「排版难看」和「音符其实是错的」这两件事分开——后者目前**尚未被证明存在**。

---

## 3. 现成服务（可能根本不用自己开发）

先验证这些能不能直接吃五线谱图片、输出「双手简谱」体裁。能 → 问题结束。

| 服务 | 能力 | 网址 |
|---|---|---|
| **虫虫钢琴** | 五线谱/简谱切换，有「转简谱」功能 | https://www.gangqinpu.com/ |
| **8pu** | 在线制谱：简谱 / 功能简谱 / 和弦谱 / 五线谱，**AI 识别转谱**，多谱式混排对照 | https://8pu.cn/ |
| **反谱** | AI 音乐转谱，输出 MusicXML / MIDI | https://miaogaichong.com/ |
| **everyonepiano** | 本身就是「转简谱」服务（同页提供两种谱） | https://everyonepiano.cn/ |
| **Musopen / 虫虫** 等 | 其他 | — |

---

## 4. 候选开源项目（用户计划 fork 这些，而非从零自建）

> 用户的判断：**算法部分若已有成熟解，fork 改造比从零做快很多**。
> 评估维度建议：算法覆盖度（多声部？和弦标记？歌词？）、许可证、维护活跃度、
> 能否接到「OMR → MusicXML」前端。

| 项目 | 形态 | 已知能力 | 备注 |
|---|---|---|---|
| [JianpuRender](https://github.com/flufy3d/JianpuRender) | npm `jianpurender`，TS/SVG，开源免费 | 歌词对位、小节号、速度标记、SVG/PNG | **未见支援多声部与和弦标记** |
| [MuseScore 4 Jianpu plugin](https://musescore.org/en/project/jianpu-numbered-notation) | 桌面端插件 | 排版最接近专业成品，含歌词 | 排版基准 |
| [lzh9102/musicxml_to_jianpu](https://github.com/lzh9102/musicxml_to_jianpu) | Python | MusicXML → 简谱 | 作者自述较基础/未完成 |
| [ssb22/jianpu-ly](https://github.com/ssb22/jianpu-ly) | Python + Lilypond | 在 Lilypond 里排简谱 | 需要 Lilypond |
| ~~OSMD Jianpu mode~~ | — | — | **sponsor 付费版，排除** |

《月下煮茶》在 musescore.com 上有现成的 Piano Solo 谱（可下载 MusicXML），
适合做「路线 C：绕开 OMR 走 MusicXML」的输入。

---

## 5. Audiveris（OMR 引擎）

官方发行：https://github.com/Audiveris/audiveris/releases

| 平台 | 产物 | 备注 |
|---|---|---|
| macOS Apple Silicon | `Audiveris-5.11.0-macosx-arm64.dmg`（81MB） | **自签未签名**；官方安装后约 162MB |
| macOS Intel | `Audiveris-5.11.0-macosx-x86_64.dmg` | 同上 |
| Linux x86_64 | `Audiveris-5.11.0-ubuntu24.04-x86_64.deb` | 装到 `/opt/audiveris/bin/Audiveris` |
| Linux aarch64 | **无** | ARM 机器（如 arm.3pm.lol）用不了官方 deb |

**命令行**（三平台一致）：
```
Audiveris -batch -export [-sheets N] -output <dir> -- <input>
```

**安装注意**：官方 dmg 未做代码签名。用浏览器下载会被 Gatekeeper 拦
（需「系统设置 → 隐私与安全性 → 仍要打开」）；用 `curl` 下载不会带 quarantine 标记，可绕过。

**本机清理记录（2026-11-24）**：已卸载
`/Applications/Audiveris.app`（162MB）+ `~/Library/Application Support/AudiverisLtd`（OCR 数据）。
需用时重新下载安装。

---

## 6. 对接背景（供协作者了解上下文）

对接人**自己写过**五线谱⇄简谱的转换程序，有实际乐理与简谱知识。

讨论音乐/记谱问题时**可直接使用乐理术语**（固定调/首调、和弦标记、分解和弦、三连音、
增时线/减时线、八度点、装饰音），不必从基础解释起。

其核心判断（已记录并采纳于 `JIANPU-CONVERSION-NOTES.md` §4.2）：
「五线谱把和弦完整写出来，简谱的伴奏部分改用和弦标记；要转换必须先理解那是什么调、
那块的音构成什么和弦」——这个判断是对的，但**只对「一般简谱」体裁成立**，
而本项目的目标体裁是「双手简谱」，不需要这一步。

---

## 7. 仓库与分支状态

```
github.com/kuniakil/Pudu          fork of youzi467/Pudu
├── main                           已 merge：移植成果 + 全部笔记
└── feat/macos-linux-port          开发分支（与 main 同步）
```

**上游 `youzi467/Pudu` 的 `feat/musicxml-basics` 是 2026-07-14 的旧分支**，
85 个 commit 前已合并进上游 main，不是新功能——不用去追。

上游另有 `to_be_delete/` 归档区（测试/评测代码被 gitignore），
所以上游宣称的「13492/13492 音符 100% 校验」外部**无法复现**；
本 fork 只补了平台层测试（`test/test_platform.cpp`，23/23 全绿）。
