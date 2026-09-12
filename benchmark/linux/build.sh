#!/usr/bin/env bash
# ============================================================================
# benchmark/linux/build.sh — 在 Linux/GCC 下构建三代引擎对抗基准
#
# 与 Windows 版（build_*.bat，VS2019 v142 /O2 /utf-8）等价：
#   - 编译单元、宏、包含路径一一对应
#   - 唯一新增的是 -I benchmark/linux（windows.h 兼容垫片）
#   - 算法源码零改动
#
# 用法：bash benchmark/linux/build.sh
# 产物：benchmark/bin/{AIBenchmark,AIBenchmarkMay,AIBenchmarkOldMay,
#                      DepthProbe,ReplayDebug,TestMill,NineChessConsole}
#       （与 Windows 构建脚本同一个输出目录，该目录已被 .gitignore 忽略）
# ============================================================================
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"

CXX="${CXX:-g++}"
OUT="$REPO/benchmark/bin"
mkdir -p "$OUT"

# -O2 对应 MSVC /O2；-fno-strict-aliasing 规避旧引擎位运算里的类型双关。
CXXFLAGS=(-O2 -std=c++20 -pthread -fno-strict-aliasing
          -I"$REPO/benchmark/linux" -I"$REPO/benchmark" -I"$REPO/NineChess/src")
LDFLAGS=(-pthread)

CUR_CORE=("$REPO/NineChess/src/ninechess.cpp"
          "$REPO/NineChess/src/ninechess_ai_ab.cpp"
          "$REPO/NineChess/src/ninechess_symmetry.cpp")
OLD_CORE=("$REPO/benchmark/old_engine/ninechess.cpp"
          "$REPO/benchmark/old_engine/ninechessai_ab.cpp")
MAY_CORE=("$REPO/benchmark/may_engine/ninechess.cpp"
          "$REPO/benchmark/may_engine/ninechess_ai_ab.cpp")

build() {
    local name="$1"; shift
    echo ">>> $name"
    "$CXX" "${CXXFLAGS[@]}" -o "$OUT/$name" "$@" "${LDFLAGS[@]}"
}

build AIBenchmark      "$REPO/benchmark/benchmark.cpp"          "${OLD_CORE[@]}" "${CUR_CORE[@]}"
build AIBenchmarkMay   "$REPO/benchmark/benchmark_may.cpp"      "${MAY_CORE[@]}" "${CUR_CORE[@]}"
build AIBenchmarkOldMay "$REPO/benchmark/benchmark_old_may.cpp" "${OLD_CORE[@]}" "${MAY_CORE[@]}"
build DepthProbe       "$REPO/benchmark/depthprobe.cpp"         "${OLD_CORE[@]}" "${CUR_CORE[@]}"
build ReplayDebug      "$REPO/benchmark/replay_debug.cpp"       "${OLD_CORE[@]}" "${CUR_CORE[@]}"
build TestMill         "$REPO/benchmark/test_oldmill.cpp"       "${OLD_CORE[@]}"
build NineChessConsole "$REPO/NineChessConsole/ninechessconsole.cpp" "${CUR_CORE[@]}" \
                       "$REPO/NineChess/src/ninechess_book.cpp"

echo
echo "构建完成，产物在 $OUT"
