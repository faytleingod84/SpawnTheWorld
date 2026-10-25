@echo off
setlocal EnableExtensions
cd /d "%~dp0"
rem Spawn The World v1.0.0 public source (dinput8.dll only).
rem Requires LLVM clang-cl, clang, lld-link and Python 3 on PATH.
where clang-cl >nul 2>nul || (echo FEHLER: clang-cl fehlt & exit /b 1)
where clang >nul 2>nul || (echo FEHLER: clang fehlt & exit /b 1)
where lld-link >nul 2>nul || (echo FEHLER: lld-link fehlt & exit /b 1)
where python >nul 2>nul || (echo FEHLER: python fehlt & exit /b 1)
if not exist build mkdir build
if not exist dist mkdir dist
python scripts\make_version_resource.py build\version.res || exit /b 1
lld-link /lib /machine:x64 /def:src\kernel32-monolith.def /out:build\kernel32.lib || exit /b 1
lld-link /lib /machine:x64 /def:src\user32-monolith.def /out:build\user32.lib || exit /b 1
clang-cl /nologo /c /O2 /GS /GR- /Zl /Fo:build\SpawnTheWorld.obj src\SpawnTheWorld.cpp || exit /b 1
clang-cl /nologo /c /O2 /GS /GR- /Zl /Fo:build\EmbeddedEnemies.obj src\EmbeddedEnemies.cpp || exit /b 1
clang-cl /nologo /c /O2 /GS /GR- /Zl /DOBS_SINGLE_DINPUT8 /Fo:build\MonolithExports.obj src\MonolithExports.cpp || exit /b 1
clang --target=x86_64-pc-windows-msvc -c src\dinput8.s -o build\dinput8-stubs.obj || exit /b 1
rem SecurityCookie.cpp MUST be /GS- (initializes cookie before protected code).
clang-cl /nologo /c /O2 /GS- /GR- /Zl /Fo:build\SecurityCookie.obj src\SecurityCookie.cpp || exit /b 1
lld-link /nologo /dll /machine:x64 /entry:DllMain /subsystem:windows /nodefaultlib /timestamp:0x6add4ff4 /def:src\dinput8.def /out:dist\dinput8.dll build\SpawnTheWorld.obj build\EmbeddedEnemies.obj build\MonolithExports.obj build\dinput8-stubs.obj build\SecurityCookie.obj build\kernel32.lib build\user32.lib build\version.res || exit /b 1
python scripts\check_build.py dist\dinput8.dll || exit /b 1
echo.
echo Fertig: dist\dinput8.dll
exit /b 0
