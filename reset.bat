@echo off

REM Path to Unreal Editor executable (adjust if different)
set unreal_editor_path="@echo off

REM Path to Unreal Editor executable (adjust if different)
set unreal_editor_path="C:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"

REM Path to the project file
set project_path="C:\Users\Sarah\Documents\Unreal Projects\TheWytching\thewytching.uproject"

REM Close Unreal Editor
taskkill /f /im UnrealEditor.exe >nul 2>&1
if %errorlevel%==0 (
    echo Unreal Editor closed.
) else (
    echo Unreal Editor was not running or failed to close.
)

REM Wait 2 seconds
timeout /t 2 /nobreak >nul

REM Restart Unreal Editor
start "" %unreal_editor_path% %project_path%
if %errorlevel%==0 (
    echo Unreal Editor restarted.
) else (
    echo Failed to restart Unreal Editor.
)
\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"

REM Path to the project file
set project_path="C:\UE_5.7\Engine\Binaries\Win64\thewytching\thewytching.uproject"

REM Close Unreal Editor
taskkill /f /im UnrealEditor.exe >nul 2>&1
if %errorlevel%==0 (
    echo Unreal Editor closed.
) else (
    echo Unreal Editor was not running or failed to close.
)

REM Wait 2 seconds
timeout /t 2 /nobreak >nul

REM Restart Unreal Editor
start "" %unreal_editor_path% %project_path%
if %errorlevel%==0 (
    echo Unreal Editor restarted.
) else (
    echo Failed to restart Unreal Editor.
)
