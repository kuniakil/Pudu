cmake_minimum_required(VERSION 3.25)

# ----------------------------------------------------------------------
# 谱渡 Pudu · 构建配置（Windows / macOS / Linux）
# ----------------------------------------------------------------------
project(Pudu VERSION 0.9.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------
# 1. vcpkg toolchain 自动发现（仅在可用时）
#    Windows 推荐走 vcpkg（vcpkg.json 声明 pugixml）。
#    macOS / Linux 不强制要求 vcpkg：优先用系统已安装的 pugixml，
#    缺失时回退到 FetchContent 源码拉取（见第 2 节）。
# ----------------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain" FORCE)
endif()

# ----------------------------------------------------------------------
# 2. 依赖：pugixml（读写 MusicXML）
#    MusicXML 本质就是 XML，用轻量 XML 库即可，无需 libmusicxml2
#    （后者未收录进 vcpkg，MSVC 源码编译也较麻烦）。
#    查找顺序：已安装（vcpkg / brew / apt） → FetchContent 源码回退
# ----------------------------------------------------------------------
find_package(pugixml CONFIG QUIET)
if(pugixml_FOUND)
    message(STATUS "pudixml: 使用已安装版本")
else()
    message(STATUS "pudixml: 未找到已安装版本，改用 FetchContent 源码拉取")
    include(FetchContent)
    FetchContent_Declare(pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        v1.14
        GIT_SHALLOW    TRUE)
    set(PUGIXML_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(pugixml)
endif()

# 与 vcpkg 默认 x64-windows 动态 triplet 保持 CRT 一致 (/MD)。
# 仅 MSVC 生效——其它平台设置该变量无意义且会被 CMake 忽略。
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ----------------------------------------------------------------------
# 3. 平台层：把 OS 专属调用收敛到 platform_win / platform_posix
# ----------------------------------------------------------------------
if(WIN32)
    set(PUDU_PLATFORM_SOURCES src/platform/platform_win.cpp)
else()
    set(PUDU_PLATFORM_SOURCES src/platform/platform_posix.cpp)
endif()

# ----------------------------------------------------------------------
# 4. 构建目标
# ----------------------------------------------------------------------
add_executable(Pudu
    src/main.cpp
    src/musicxml_parser.cpp
    src/jianpu_converter.cpp
    src/transpose.cpp
    src/jianpu_to_staff.cpp       # 阶段 3 G1：JianpuDoc -> Score
    src/musicxml_serializer.cpp   # 阶段 3 G2：Score -> MusicXML
    src/jianpu_text_parser.cpp    # G4：简谱文本输入解析（L1 文本 -> JianpuDoc）
    src/omr_adapter.cpp           # 阶段1 OMR 黑盒集成适配层
    src/jianpu_postcorrect.cpp    # P1-1：后处理音乐规则引擎（确定性自修/标记）
    ${PUDU_PLATFORM_SOURCES}
)

# 头文件目录（score_model.hpp / musicxml_parser.hpp / platform/ 位于 include/）
target_include_directories(Pudu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 强制 MSVC 以 UTF-8 读取源码（源码含中文注释，默认按代码页936/GBK解析会触发
# C4819 并导致标识符被误判为"未声明"，进而连锁报错。加此选项后彻底消除）
if(MSVC)
    target_compile_options(Pudu PRIVATE /utf-8)
endif()

# 注入 tools/ 绝对路径（转正斜杠，避免 Windows 反斜杠在字符串字面量成非法转义），
# 供 omr_adapter 的子进程引擎（oemer/audiveris 预设）定位运行器脚本
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools" PUDU_TOOLS_DIR_FWD)
target_compile_definitions(Pudu PRIVATE "PUDU_TOOLS_DIR=\"${PUDU_TOOLS_DIR_FWD}\"")

target_link_libraries(Pudu PRIVATE
    pugixml::pugixml
)

# ----------------------------------------------------------------------
# 5. 单元测试目标（header-only 自研框架，零外部依赖）
#    运行：./build/<preset>/PuduTests
#    （退出码：全绿 0 / 有失败非 0）
# ----------------------------------------------------------------------
enable_testing()
add_executable(PuduTests
    test/pudu_test_main.cpp
    test/test_jianpu_render_config.cpp
    src/jianpu_converter.cpp
    src/musicxml_parser.cpp
    ${PUDU_PLATFORM_SOURCES}
)
target_include_directories(PuduTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(PuduTests PRIVATE /utf-8)
endif()
target_link_libraries(PuduTests PRIVATE pugixml::pugixml)
add_test(NAME pudu_render_config COMMAND PuduTests)

# ----------------------------------------------------------------------
# 6. 安装
# ----------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS Pudu RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
