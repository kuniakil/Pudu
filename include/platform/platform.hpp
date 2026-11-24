// ----------------------------------------------------------------------
// 谱渡 Pudu · 跨平台抽象层
//
// 把 OS 专属的操作系统调用收敛到这里，让上层代码（omr_adapter /
// musicxml_parser / main）与平台无关：
//   * 子进程执行（捕获 stdout+stderr + 超时杀进程）
//   * 环境变量读取
//   * 文件存在性判断
//   * XML 文件路径类型（Windows 需宽字符，POSIX 原生 UTF-8）
//
// Windows 实现见 src/platform/platform_win.cpp，
// POSIX (macOS/Linux) 实现见 src/platform/platform_posix.cpp。
// 两侧行为对齐：退出码语义、错误串、输出合并方式一致。
// ----------------------------------------------------------------------
#pragma once

#include <string>

namespace pudu {
namespace platform {

// ---------------------------------------------------------------------
// 子进程执行结果
// ---------------------------------------------------------------------
struct ProcResult {
    // 子进程退出码。约定值：
    //   -2 = 超时（已强杀子进程）
    //   -3 = 启动失败（fork/exec 失败）或被信号异常终止
    int exitCode = -1;
    // stdout + stderr 合并输出
    std::string output;
    // 失败原因（超时 / 启动失败时非空）
    std::string error;
};

// 运行一条命令行并捕获 stdout+stderr 合并流。
// cmd 为「已正确引号」的命令行字符串（路径用双引号包裹），
// 与 Windows CreateProcessA 的解析口径一致。
// timeoutMs < 0 表示不设超时。
ProcResult runProcess(const std::string& cmd, int timeoutMs);

// 读取环境变量；未设置时返回空串。
std::string envGet(const char* name);

// 用户主目录：Windows 取 USERPROFILE，POSIX 取 HOME；未设置时返回空串。
std::string homeDir();

// 当前可执行文件所在目录（不含尾部分隔符）；取不到时返回空串。
// 用于「便携发行」布局下的资源定位（tools/ 与二进制同级）。
std::string executableDir();

// 文件是否存在。
bool fileExists(const std::string& path);

// XML 文件路径类型。
// Windows 上 ANSI fopen 在中文路径下会失败，故需宽字符；
// POSIX 的文件名原生就是字节流（UTF-8），直接用 std::string。
#ifdef _WIN32
using XmlPath = std::wstring;
#else
using XmlPath = std::string;
#endif

// 把 UTF-8 路径转换为 XmlPath。
XmlPath toXmlPath(const std::string& utf8Path);

} // namespace platform
} // namespace pudu
