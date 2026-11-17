@echo off
where g++ >nul 2>nul
if %errorlevel%==0 (
  g++ -std=c++11 -O2 -mwindows -static -static-libgcc -static-libstdc++ src\main_win32.cpp -o TRex.exe -lwinmm -lgdi32
) else (
  cl /nologo /EHsc /O2 src\main_win32.cpp /Fe:TRex.exe user32.lib gdi32.lib winmm.lib /link /SUBSYSTEM:WINDOWS
)
echo Done.
