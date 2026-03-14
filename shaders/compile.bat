@echo OFF

REM This is TEMPORARY
REM Ideally shader compilation should be integrated into CMake

set WKBRE_SHADER_OUT=%~dp0\..\redata

call :compile vs_4_0 VS
call :compile vs_4_0 VS_Fog
call :compile ps_4_0 PS
call :compile ps_4_0 PS_AlphaTest
call :compile ps_4_1 PS_Map
call :compile ps_4_0 PS_Lake
goto :EOF

:compile
echo ====== Compiling %2 (%1) ======
C:\Apps\dxc_2025_07_14\bin\x64\dxc.exe -spirv -T %1 -E %2 -Fo %WKBRE_SHADER_OUT%\EnhBasicShader_%2.spirv EnhBasicShader.hlsl
goto :EOF
