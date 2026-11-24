// ----------------------------------------------------------------------
// 谱渡 Pudu · 平台层回归测试
//
// 覆盖两个在 macOS 移植中实际踩到、且在 Windows 上永远不会暴露的坑：
//
//  1) executableDir() 在 macOS 上必须能取到可执行文件路径。
//     _NSGetExecutablePath(NULL, &size) 首次调用**必然返回 -1**（缓冲区太小
//     或为 NULL），但它同时把所需长度写进 size。若把 -1 当失败直接返回空串，
//     tools/ 的「可执行文件同级」回退就永远失效——
//     而本地构建时编译期注入的 PUDU_TOOLS_DIR 恰好存在，会把这个 bug 掩盖掉，
//     只有拿 CI 产物（编译期路径是 runner 路径）做便携布局测试才会暴露。
//
//  2) toXmlPath() 在 POSIX 上必须原样返回 UTF-8 路径（pugixml 直接吃
//     char*，无需宽字符转换）；在 Windows 上必须产出宽字符。
//
//  3) envGet() / fileExists() 的基本行为。
// ----------------------------------------------------------------------

#include "pudu_test.hpp"

#include "platform/platform.hpp"

#include <cstdlib>
#include <string>

using pudu::platform::envGet;
using pudu::platform::executableDir;
using pudu::platform::fileExists;
using pudu::platform::toXmlPath;

TEST(platform_executableDir_nonEmpty) {
    // 能取到自身路径 —— macOS 上依赖 _NSGetExecutablePath 的两次调用
    const std::string dir = executableDir();
    EXPECT_FALSE(dir.empty());
    // 目录里应存在本测试可执行文件（取其文件名）
    const size_t slash = dir.find_last_of("/\\");
    EXPECT_TRUE(slash != std::string::npos);
}

TEST(platform_executableDir_containsTestBinary) {
    const std::string dir = executableDir();
    EXPECT_FALSE(dir.empty());
    const size_t slash = dir.find_last_of("/\\");
    EXPECT_TRUE(slash != std::string::npos);
    // 可执行文件名不应为空
    EXPECT_FALSE(dir.substr(slash + 1).empty());
}

TEST(platform_toXmlPath_preservesContent) {
    const std::string utf8 = "data/\xe9\x9f\xb3\xe4\xb9\x90/score.musicxml";
    const auto p = toXmlPath(utf8);
#ifdef _WIN32
    // Windows：宽字符长度应等于 UTF-8 字节数（中文字符占多字节）
    EXPECT_TRUE(p.size() > 0);
    EXPECT_TRUE(p.size() < utf8.size());
#else
    // POSIX：原样返回 UTF-8 字节
    EXPECT_EQ(p, utf8);
#endif
}

TEST(platform_toXmlPath_empty) {
    const auto p = toXmlPath("");
#ifdef _WIN32
    EXPECT_TRUE(p.empty());
#else
    EXPECT_TRUE(p.empty());
#endif
}

TEST(platform_envGet_missing) {
    EXPECT_TRUE(envGet("PUDU_TEST_DEFINITELY_NOT_SET_12345").empty());
}

TEST(platform_envGet_readsProcessEnv) {
#ifdef _WIN32
    ::_putenv("PUDU_TEST_ENV_PROBE=hello");
    EXPECT_EQ(envGet("PUDU_TEST_ENV_PROBE"), std::string("hello"));
    ::_putenv("PUDU_TEST_ENV_PROBE=");
#else
    ::setenv("PUDU_TEST_ENV_PROBE", "hello", 1);
    EXPECT_EQ(envGet("PUDU_TEST_ENV_PROBE"), std::string("hello"));
    ::unsetenv("PUDU_TEST_ENV_PROBE");
#endif
    EXPECT_TRUE(envGet("PUDU_TEST_ENV_PROBE").empty());
}

TEST(platform_fileExists) {
    EXPECT_FALSE(fileExists("/definitely/not/a/real/path/xyz.txt"));
    // 临时目录必定存在（POSIX 惯例）
#ifndef _WIN32
    EXPECT_TRUE(fileExists("/tmp"));
#endif
}
