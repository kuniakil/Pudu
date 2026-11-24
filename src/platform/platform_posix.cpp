// ----------------------------------------------------------------------
// 谱渡 Pudu · 平台层 · POSIX (macOS / Linux) 实现
//
// 与 platform_win.cpp 行为对齐：
//   * 输出 = stdout + stderr 合并流
//   * exitCode 语义一致（-2 超时 / -3 启动失败）
//   * timeoutMs < 0 表示不设超时
//
// 实现要点：
//   * fork + execvp，子进程把 stdout/stderr 都 dup2 到同一管道写端
//   * 父进程用 poll() 边读边等，避免子进程写满管道后死锁
//   * 超时用 SIGKILL 强杀并回收，避免留下僵尸进程
// ----------------------------------------------------------------------

#include "platform/platform.hpp"

#ifndef _WIN32

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace pudu {
namespace platform {

namespace {

// 把命令行字符串切成 argv：支持双引号包裹（路径含空格时必需）。
// 与 Windows CreateProcessA 的解析口径对齐——本项目所有调用点都用
// 双引号包裹路径，故这里只需处理双引号，不处理反斜杠转义。
std::vector<std::string> splitCommandLine(const std::string& cmd) {
    std::vector<std::string> args;
    std::string cur;
    bool inQuote = false;
    bool hasToken = false;
    for (char c : cmd) {
        if (c == '"') {
            inQuote = !inQuote;
            hasToken = true;   // "" 也算一个（空）参数
            continue;
        }
        if (!inQuote && (c == ' ' || c == '\t')) {
            if (hasToken) {
                args.push_back(cur);
                cur.clear();
                hasToken = false;
            }
            continue;
        }
        cur.push_back(c);
        hasToken = true;
    }
    if (hasToken) args.push_back(cur);
    return args;
}

} // namespace

ProcResult runProcess(const std::string& cmd, int timeoutMs) {
    ProcResult res;

    std::vector<std::string> args = splitCommandLine(cmd);
    if (args.empty()) {
        res.exitCode = -3;
        res.error = "空命令行";
        return res;
    }
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (std::string& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    int pipefd[2] = {-1, -1};
    if (::pipe(pipefd) != 0) {
        res.exitCode = -3;
        res.error = std::string("pipe 失败: ") + std::strerror(errno);
        return res;
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        res.exitCode = -3;
        res.error = std::string("fork 失败: ") + std::strerror(errno);
        return res;
    }

    if (pid == 0) {
        // ---- 子进程 ----
        ::close(pipefd[0]);
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        ::close(pipefd[1]);
        ::execvp(argv[0], argv.data());
        // exec 失败：父进程会读到这段文字并看到非零退出码
        const char* msg = "[pudu] execvp 失败\n";
        ssize_t ignored = ::write(STDERR_FILENO, msg, std::strlen(msg));
        (void)ignored;
        ::_exit(127);
    }

    // ---- 父进程 ----
    ::close(pipefd[1]);

    const bool hasTimeout = (timeoutMs >= 0);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(hasTimeout ? timeoutMs : 0);

    bool timedOut = false;
    char buf[4096];
    for (;;) {
        int waitMs = -1;   // -1 = 无限等待
        if (hasTimeout) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) { timedOut = true; break; }
            auto remain = deadline - now;
            waitMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(remain).count());
            if (waitMs < 0) waitMs = 0;
        }

        struct pollfd pfd {};
        pfd.fd = pipefd[0];
        pfd.events = POLLIN;
        int pr = ::poll(&pfd, 1, waitMs);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pr == 0) {   // poll 超时
            timedOut = true;
            break;
        }
        ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
        if (n > 0) {
            res.output.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) break;   // EOF：子进程已关闭写端
        if (errno == EINTR) continue;
        break;
    }
    ::close(pipefd[0]);

    if (timedOut) {
        ::kill(pid, SIGKILL);
        int st = 0;
        while (::waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
        res.exitCode = -2;
        res.error = "引擎超时(" + std::to_string(timeoutMs) + "ms)";
        return res;
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(status)) {
        res.exitCode = WEXITSTATUS(status);
    } else {
        res.exitCode = -3;
        res.error = "子进程被信号终止";
    }
    return res;
}

std::string envGet(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

std::string homeDir() { return envGet("HOME"); }

std::string executableDir() {
    char buf[4096];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
#ifdef __APPLE__
        // macOS 无 /proc；用 dyld 取自身路径
        uint32_t size = 0;
        if (_NSGetExecutablePath(nullptr, &size) != 0 || size == 0) return {};
        std::vector<char> tmp(size);
        if (_NSGetExecutablePath(tmp.data(), &size) != 0) return {};
        n = static_cast<ssize_t>(size);
        std::memcpy(buf, tmp.data(), std::min<size_t>(size, sizeof(buf) - 1));
#else
        return {};
#endif
    }
    buf[n] = '\0';
    std::string p(buf);
    size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? std::string() : p.substr(0, slash);
}

bool fileExists(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0;
}

XmlPath toXmlPath(const std::string& utf8Path) { return utf8Path; }

} // namespace platform
} // namespace pudu

#endif // !_WIN32
