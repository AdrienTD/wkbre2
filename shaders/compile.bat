@echo OFF

REM This is TEMPORARY
REM Ideally shader compilation should be integrated into CMake

set WKBRE_SHADER_OUT=%~dp0\..\redata

call :compile EnhBasicShader vs_4_0 VS
call :compile EnhBasicShader vs_4_0 VS_Fog
call :compile EnhBasicShader ps_4_0 PS
call :compile EnhBasicShader ps_4_0 PS_AlphaTest
call :compile EnhBasicShader ps_4_1 PS_Map
call :compile EnhBasicShader ps_4_0 PS_Lake

call :compile EnhSceneShader vs_4_0 VS
call :compile EnhSceneShader vs_4_0 VS_Anim
call :compile EnhSceneShader ps_4_0 PS
call :compile EnhSceneShader ps_4_0 PS_Alpha

goto :EOF

:compile
echo ====== Compiling %1_%3 (%2) ======
C:\Apps\dxc_2025_07_14\bin\x64\dxc.exe -spirv -T %2 -E %3 -Fo %WKBRE_SHADER_OUT%\%1_%3.spirv %1.hlsl
goto :EOF
