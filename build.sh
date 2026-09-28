#!/usr/bin/env bash
# Linux / macOS 上用 MinGW-w64 交叉编译 Windows x64 版本，并运行可移植单元测试。
# 依赖：x86_64-w64-mingw32-g++、x86_64-w64-mingw32-windres、g++
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}

echo "== 单元测试 =="
for t in logic animation motion_flow blit; do
  g++ -std=c++17 -O2 -Wall -Wextra "tests/test_$t.cpp" -o "build/test_$t"
  "./build/test_$t" assets/motion-vectors.bin
done

echo "== Windows x64 =="
"$WINDRES" -I . -I src -O coff src/pet.rc -o build/pet-res.o
"$CXX" -std=c++17 -O2 -s -municode -mwindows -static -static-libgcc -static-libstdc++ \
  -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -Wall -Wextra \
  -Isrc src/main.cpp src/app.cpp src/pet_window.cpp src/panel.cpp src/system.cpp src/sprites.cpp src/settings.cpp \
  build/pet-res.o -o build/LiliDesktopPet.exe \
  -lgdiplus -lole32 -loleaut32 -lshell32 -luser32 -lgdi32 -luuid -ladvapi32
ls -l build/LiliDesktopPet.exe
