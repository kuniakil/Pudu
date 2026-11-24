// ----------------------------------------------------------------------
// 谱渡 Pudu · 阶段1 OMR 黑盒集成 · 适配层实现 (omr_adapter.cpp)
//
// 见 include/omr_adapter.hpp 的设计说明。零模型改动：本文件不 include
// 任何既有数据模型头，仅通过文件系统把 MusicXML 交给下游解析器。
// ----------------------------------------------------------------------

#include "omr_adapter.hpp"
#include "platform/platform.hpp"

#include <array>
#include <fstream>
#include <string>
#include <vector>

namespace pudu {

// 内嵌 fixture 样例：一份合法、确定的 MusicXML（C 大调 4/4，
// "小星星"前两句 C C G G A A G）。作为 fixture 引擎的"OMR 产出"，
// 用于 ctest 与 CLI 演示，证明全链路契约正确，且不依赖任何外部进程/网络。
static const char* kOmrFixtureMusicXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <work><work-title>OMR Fixture Sample</work-title></work>
  <part-list>
    <score-part id="P1"><part-name>Music</part-name></score-part>
  </part-list>
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>1</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note><pitch><step>C</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>
      <note><pitch><step>C</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>
      <note><pitch><step>G</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>
      <note><pitch><step>G</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>
    </measure>
    <measure number="2">
      <note><pitch><step>A</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>
      <note><pitch><step>A</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>
      <note><pitch><step>G</step><octave>4</octave></pitch><duration>2</duration><type>half</type></note>
    </measure>
  </part>
</score-partwise>)";

// 运行命令，捕获 stdout+stderr 到 output。返回进程退出码；
// 超时返回 -2；创建失败返回 -3。err 含失败原因。
// 实现委托给平台层（Windows: CreateProcess；POSIX: fork/execvp + poll）。
static int runCommand(const std::string& cmd, int timeoutMs,
                      std::string& output, std::string& err) {
    pudu::platform::ProcResult r = pudu::platform::runProcess(cmd, timeoutMs);
    output = std::move(r.output);
    err = std::move(r.error);
    return r.exitCode;
}

// 探测某 python 解释器能否 import oemer（用于自动选址）
static bool pythonCanImportOemer(const std::string& pyExe) {
    if (pyExe.empty()) return false;
    std::string out, err;
    int rc = runCommand("\"" + pyExe + "\" -c \"import oemer\"", 30000, out, err);
    return rc == 0;
}

// 探测某 python 解释器能否启动（不要求装任何包）
static bool pythonCanRun(const std::string& pyExe) {
    if (pyExe.empty()) return false;
    std::string out, err;
    int rc = runCommand("\"" + pyExe + "\" -c \"pass\"", 30000, out, err);
    return rc == 0;
}

// python 解释器候选（按平台惯例排序）
static std::vector<std::string> pythonCandidates(const OmrEngineConfig& cfg) {
    std::vector<std::string> cands;
    if (cfg.pythonExplicit && !cfg.python.empty() && cfg.python != "python")
        cands.push_back(cfg.python);
    std::string envPy = pudu::platform::envGet("PUDU_OMR_PYTHON");
    if (!envPy.empty()) cands.push_back(envPy);
#ifdef _WIN32
    cands.push_back("python");
#else
    // POSIX 惯例是 python3；多数发行版不再提供无版本号的 python
    cands.push_back("python3");
    cands.push_back("python");
#endif
    std::string home = pudu::platform::homeDir();
    if (!home.empty()) {
#ifdef _WIN32
        cands.push_back(home + "\\.workbuddy\\binaries\\python\\envs\\default\\Scripts\\python.exe");
#else
        cands.push_back(home + "/.workbuddy/binaries/python/bin/python3");
#endif
    }
    return cands;
}

// 解析可用于 oemer 的 python（见头文件声明）
std::string resolveOmerPython(const OmrEngineConfig& cfg) {
    if (cfg.pythonExplicit && !cfg.python.empty() && cfg.python != "python")
        return cfg.python;
    for (const auto& c : pythonCandidates(cfg)) {
        if (pythonCanImportOemer(c)) return c;
    }
    return cfg.python; // 兜底（原默认），错误信息据此生成
}

// 解析「能启动」的 python（不要求装 oemer）
std::string resolvePython(const OmrEngineConfig& cfg) {
    if (cfg.pythonExplicit && !cfg.python.empty())
        return cfg.python;
    for (const auto& c : pythonCandidates(cfg)) {
        if (pythonCanRun(c)) return c;
    }
    return cfg.python;
}

// 写内嵌 fixture 样例到 out（C++ 原生，确定性）
static bool writeEmbeddedFixture(const std::string& out, std::string& err) {
    std::ofstream f(out, std::ios::binary);
    if (!f) {
        err = "无法写入 fixture 输出: " + out;
        return false;
    }
    f << kOmrFixtureMusicXml;
    if (!f) {
        err = "写入 fixture 输出失败: " + out;
        return false;
    }
    return true;
}

// 检查产物存在且非空
static bool outputLooksValid(const std::string& out, std::string& err) {
    std::ifstream f(out, std::ios::binary);
    if (!f) {
        err = "OMR 产出文件不存在: " + out;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    if (content.find("<score-partwise") == std::string::npos &&
        content.find("<score-timewise") == std::string::npos) {
        err = "OMR 产出不是合法 MusicXML（缺 score-partwise/timewise 根）: " + out;
        return false;
    }
    return true;
}

bool runOmr(const std::string& input, const std::string& outMusicXml,
            const OmrEngineConfig& cfg, std::string& err) {
    if (cfg.engine == "fixture") {
        // C++ 原生 fixture：确定性，无外部依赖
        return writeEmbeddedFixture(outMusicXml, err) &&
               outputLooksValid(outMusicXml, err);
    }

    // 子进程引擎：构建命令模板（{input}/{output} 占位已内联替换）
    std::string cmd;
    if (cfg.engine == "oemer") {
        if (cfg.toolsDir.empty()) {
            err = "oemer 引擎需设置 toolsDir（CMake 注入 PUDU_TOOLS_DIR）";
            return false;
        }
        // P0-2：开关关时脚本名恒为 omr_oemer.py，命令串与 P0-2 之前逐字节一致
        const char* script = cfg.preprocess ? "/omr_pipeline.py" : "/omr_oemer.py";
        cmd = "\"" + cfg.python + "\" \"" + cfg.toolsDir + script + "\" \"" +
              input + "\" \"" + outMusicXml + "\"";
    } else if (cfg.engine == "audiveris") {
        if (cfg.toolsDir.empty()) {
            err = "audiveris 引擎需设置 toolsDir（CMake 注入 PUDU_TOOLS_DIR）";
            return false;
        }
        cmd = "\"" + cfg.python + "\" \"" + cfg.toolsDir + "/omr_audiveris.py\" \"" +
              input + "\" \"" + outMusicXml + "\"";
    } else {
        err = "未知 OMR 引擎: " + cfg.engine +
              "（可选: oemer | audiveris | fixture）";
        return false;
    }

    std::string output, rerr;
    int rc = runCommand(cmd, cfg.timeoutMs, output, rerr);
    if (rc != 0) {
        err = "OMR 引擎退出码 " + std::to_string(rc) +
              (rerr.empty() ? "" : ("; " + rerr)) +
              (output.empty() ? "" : ("\n--- stdout/stderr ---\n" + output));
        return false;
    }
    return outputLooksValid(outMusicXml, err);
}

bool isOmrEngineAvailable(const OmrEngineConfig& cfg, std::string& detail) {
    if (cfg.engine == "fixture") {
        detail = "内置 fixture 引擎（C++ 原生，确定性，无外部依赖）";
        return true;
    }
    if (cfg.engine == "oemer") {
        if (cfg.toolsDir.empty()) {
            detail = "oemer 引擎未配置 toolsDir";
            return false;
        }
        std::string py = resolveOmerPython(cfg);
        std::string cmd = "\"" + py + "\" -c \"import oemer; print('ok')\"";
        std::string output, err;
        int rc = runCommand(cmd, 30000, output, err);
        if (rc == 0) {
            detail = "oemer 已安装（python=" + py + " 可 import oemer）";
            return true;
        }
        detail = "oemer 不可用 (python=" + py + "): 退出码 " + std::to_string(rc) +
                 (err.empty() ? "" : ("; " + err));
        return false;
    }
    if (cfg.engine == "audiveris") {
        // AV exe 定位在 Python 侧经 env PUDU_AUDIVERIS_EXE / 平台默认路径，C++ 不碰。
        // C++ 侧只需确认：适配层脚本在位 + 有可用的 python 解释器。
        if (cfg.toolsDir.empty()) {
            detail = "audiveris 引擎未配置 toolsDir";
            return false;
        }
        std::string script = cfg.toolsDir + "/omr_audiveris.py";
        if (!pudu::platform::fileExists(script)) {
            detail = "audiveris 适配层缺失: " + script;
            return false;
        }
        std::string py = resolvePython(cfg);
        if (!pythonCanRun(py)) {
            detail = "audiveris 引擎需可用的 python 解释器，当前不可用 (python=" +
                     py + ")";
            return false;
        }
        detail = "audiveris 可用（toolsDir=" + cfg.toolsDir + "，python=" + py + "）";
        return true;
    }
    detail = "未知引擎: " + cfg.engine;
    return false;
}

} // namespace pudu
