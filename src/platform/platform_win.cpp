// ----------------------------------------------------------------------
// 谱渡 Pudu · 平台层 · Windows 实现
//
// 本文件为上游原始 Windows 行为的逐字移植，保证 Windows 上的输出与
// 移植前完全一致（不引入任何行为差异）。
// ----------------------------------------------------------------------

#include "platform/platform.hpp"

#ifdef _WIN32

#include <windows.h>

namespace pudu {
namespace platform {

ProcResult runProcess(const std::string& cmd, int timeoutMs) {
    ProcResult res;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        res.exitCode = -3;
        res.error = "CreatePipe 失败";
        return res;
    }

    STARTUPINFOA si{sizeof(si)};
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    // CreateProcessA 会就地修改命令行，故用可写副本
    std::string cmdline = cmd;
    BOOL ok = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    // 关闭我们这侧的写端（子进程持有自己的副本，其关闭后管道读端才会 EOF）
    CloseHandle(hWrite);

    if (!ok) {
        DWORD e = GetLastError();
        CloseHandle(hRead);
        res.exitCode = -3;
        res.error = "CreateProcess 失败, code=" + std::to_string(e);
        return res;
    }

    // 持续读取子进程输出，避免管道写满死锁
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &n, nullptr) && n > 0)
        res.output.append(buf, n);
    CloseHandle(hRead);

    DWORD waitRes = WaitForSingleObject(
        pi.hProcess, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
    if (waitRes == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        res.exitCode = -2;
        res.error = "引擎超时(" + std::to_string(timeoutMs) + "ms)";
        return res;
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    res.exitCode = static_cast<int>(exitCode);
    return res;
}

std::string envGet(const char* name) {
    char buf[1024];
    DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) return std::string(buf, n);
    return {};
}

std::string homeDir() { return envGet("USERPROFILE"); }

bool fileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

XmlPath toXmlPath(const std::string& utf8Path) {
    if (utf8Path.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, &w[0], n);
    return w;
}

} // namespace platform
} // namespace pudu

#endif // _WIN32
