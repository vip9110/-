@echo off
rem 在 Windows 上用 MinGW-w64 编译（把 MinGW 的 bin 目录加入 PATH 后双击运行）。
setlocal
cd /d "%~dp0"
where g++ >nul 2>nul || (echo 请先安装 MinGW-w64，并把它的 bin 目录加入 PATH。 & pause & exit /b 1)
if not exist build mkdir build
windres -I . -I src -O coff src\pet.rc -o build\pet-res.o || goto :fail
g++ -std=c++17 -O2 -s -municode -mwindows -static -static-libgcc -static-libstdc++ -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -Isrc src\main.cpp src\app.cpp src\pet_window.cpp src\panel.cpp src\system.cpp src\sprites.cpp src\settings.cpp build\pet-res.o -o build\LiliDesktopPet.exe -lgdiplus -lole32 -loleaut32 -lshell32 -luser32 -lgdi32 -luuid -ladvapi32 || goto :fail
echo 已生成 build\LiliDesktopPet.exe
pause
exit /b 0
:fail
echo 编译失败。
pause
exit /b 1
