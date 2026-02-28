@echo off
REM ============================================================
REM LOCKDOWN SCRIPT - Run this ONCE before rebuild begins
REM Tags current state, sets up LFS, pushes to remote
REM ============================================================

cd /d "C:\Users\Sarah\Documents\Unreal Projects\TheWytching"

echo.
echo === Step 1: Verify Git LFS is installed ===
git lfs version
if %errorlevel% neq 0 (
    echo ERROR: Git LFS is not installed. Install from https://git-lfs.com
    echo Then re-run this script.
    pause
    exit /b 1
)

echo.
echo === Step 2: Initialize LFS tracking ===
git lfs install
git lfs track "*.uasset" "*.umap" "*.uexp"

echo.
echo === Step 3: Stage lockdown files ===
git add .gitattributes
git add .gitignore

echo.
echo === Step 4: Commit lockdown changes ===
git commit -m "Lockdown: Enable Git LFS for Content/, remove Content/ from .gitignore" -m "Content/ binary assets will now be tracked via Git LFS." -m "This prevents future data loss from untracked local-only assets." -m "" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"

echo.
echo === Step 5: Tag current state as pre-rebuild baseline ===
git tag -a pre-rebuild-baseline -m "Pre-rebuild baseline: All C++ intact, Content/ empty, ready for rebuild"

echo.
echo === Step 6: Push to remote (with tags) ===
git push origin main --tags

echo.
echo === Step 7: Verify ===
echo.
echo --- Current tags ---
git tag -l
echo.
echo --- LFS tracked patterns ---
git lfs track
echo.
echo --- Git status ---
git status

echo.
echo ============================================================
echo LOCKDOWN COMPLETE
echo - .gitattributes created (LFS patterns)
echo - .gitignore updated (Content/ now tracked)  
echo - Tagged: pre-rebuild-baseline
echo - Pushed to origin/main
echo.
echo Content/ is empty. Rebuild assets will be tracked via LFS.
echo ============================================================
pause
