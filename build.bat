@echo off
setlocal
cd /d "%~dp0"

if not exist "build\CMakeCache.txt" (
  cmake -S . -B build -G "Visual Studio 18 2026" -A x64
  if errorlevel 1 exit /b 1
)

cmake --build build --config Release --target raycast_standalone
if errorlevel 1 exit /b 1

copy /Y "build\Release\raycast_standalone.exe" "raycast_standalone.exe" >nul
if errorlevel 1 (
  echo [build] compile OK. No se pudo copiar a raiz ^(exe en uso?^). Cierra raycast_standalone.exe y vuelve a copiar.
  exit /b 0
)

echo [build] OK -^> raycast_standalone.exe
endlocal
