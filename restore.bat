@echo off
REM ============================================================
REM  restore.bat — Copy usable assets from Wytcherly2 into TheWytching
REM  Run from: C:\Users\Sarah\Documents\Unreal Projects\TheWytching\
REM  Close Unreal Editor BEFORE running this script.
REM ============================================================

set SRC=C:\Users\Sarah\Documents\Unreal Projects\Wytcherly2\Content
set DST=C:\Users\Sarah\Documents\Unreal Projects\TheWytching\Content

echo.
echo ============================================================
echo  TheWytching Content Restore from Wytcherly2
echo ============================================================
echo  Source: %SRC%
echo  Dest:   %DST%
echo.
echo  CLOSE UNREAL EDITOR BEFORE CONTINUING.
echo ============================================================
echo.
pause

REM ── Phase 2: Game Mode + Player Controller ──────────────────
echo.
echo [Phase 2] Copying GM_Sandbox, PC_Sandbox...
if not exist "%DST%\Blueprints" mkdir "%DST%\Blueprints"
copy /Y "%SRC%\Blueprints\GM_Sandbox.uasset" "%DST%\Blueprints\" >nul && echo   OK: GM_Sandbox.uasset
copy /Y "%SRC%\Blueprints\PC_Sandbox.uasset" "%DST%\Blueprints\" >nul && echo   OK: PC_Sandbox.uasset

REM ── Phase 3: Level ──────────────────────────────────────────
echo.
echo [Phase 3] Copying NPCLevel...
if not exist "%DST%\Levels" mkdir "%DST%\Levels"
copy /Y "%SRC%\Levels\NPCLevel.umap" "%DST%\Levels\" >nul && echo   OK: NPCLevel.umap

REM ── AI Controller base class ────────────────────────────────
echo.
echo [AI] Copying AIC_NPC_SmartObject...
if not exist "%DST%\Blueprints\AI" mkdir "%DST%\Blueprints\AI"
copy /Y "%SRC%\Blueprints\AI\AIC_NPC_SmartObject.uasset" "%DST%\Blueprints\AI\" >nul && echo   OK: AIC_NPC_SmartObject.uasset

REM ── AI StateTree assets ─────────────────────────────────────
echo.
echo [AI] Copying StateTree assets...
if not exist "%DST%\Blueprints\AI\StateTree" mkdir "%DST%\Blueprints\AI\StateTree"
xcopy /Y /Q "%SRC%\Blueprints\AI\StateTree\*.uasset" "%DST%\Blueprints\AI\StateTree\" >nul
echo   OK: ST_NPC_SandboxCharacter_Patrol_Subtree, ST_NPC_SandboxCharacter_SmartObject

if not exist "%DST%\Blueprints\AI\StateTree\TasksAndConditions" mkdir "%DST%\Blueprints\AI\StateTree\TasksAndConditions"
xcopy /Y /Q "%SRC%\Blueprints\AI\StateTree\TasksAndConditions\*.uasset" "%DST%\Blueprints\AI\StateTree\TasksAndConditions\" >nul
echo   OK: STT_FindRandomLocation, STT_FocusToTarget, STT_ClearFocus, etc.

REM ── SmartObject infrastructure ──────────────────────────────
echo.
echo [SmartObjects] Copying SmartObject assets...
if not exist "%DST%\Blueprints\SmartObjects" mkdir "%DST%\Blueprints\SmartObjects"
xcopy /Y /Q "%SRC%\Blueprints\SmartObjects\*.uasset" "%DST%\Blueprints\SmartObjects\" >nul
echo   OK: BP_SmartObject_Base, AC_SmartObjectAnimation, etc.

if not exist "%DST%\Blueprints\SmartObjects\Bench" mkdir "%DST%\Blueprints\SmartObjects\Bench"
xcopy /Y /Q "%SRC%\Blueprints\SmartObjects\Bench\*.uasset" "%DST%\Blueprints\SmartObjects\Bench\" >nul
echo   OK: BP_SmartBench, SO_BenchDefinition, ST_SmartObject_Bench, etc.

if not exist "%DST%\Blueprints\SmartObjects\TasksAndConditions" mkdir "%DST%\Blueprints\SmartObjects\TasksAndConditions"
xcopy /Y /Q "%SRC%\Blueprints\SmartObjects\TasksAndConditions\*.uasset" "%DST%\Blueprints\SmartObjects\TasksAndConditions\" >nul
echo   OK: STT_FindSmartObject, STT_ClaimSlot, STT_UseSmartObject, etc.

REM ── Worker characters (placeholders) ────────────────────────
echo.
echo [Workers] Copying worker BPs + AnimBPs...
copy /Y "%SRC%\Blueprints\SandboxCharacter_Mover.uasset" "%DST%\Blueprints\" >nul && echo   OK: SandboxCharacter_Mover.uasset
copy /Y "%SRC%\Blueprints\SandboxCharacter_Mover_ABP.uasset" "%DST%\Blueprints\" >nul && echo   OK: SandboxCharacter_Mover_ABP.uasset
copy /Y "%SRC%\Blueprints\SandboxCharacter_CMC.uasset" "%DST%\Blueprints\" >nul && echo   OK: SandboxCharacter_CMC.uasset
copy /Y "%SRC%\Blueprints\SandboxCharacter_CMC_ABP.uasset" "%DST%\Blueprints\" >nul && echo   OK: SandboxCharacter_CMC_ABP.uasset

REM ── Blueprint interfaces ────────────────────────────────────
echo.
echo [Interfaces] Copying BPI assets...
copy /Y "%SRC%\Blueprints\BPI_SandboxCharacter_ABP.uasset" "%DST%\Blueprints\" >nul && echo   OK: BPI_SandboxCharacter_ABP.uasset
copy /Y "%SRC%\Blueprints\BPI_SandboxCharacter_Pawn.uasset" "%DST%\Blueprints\" >nul && echo   OK: BPI_SandboxCharacter_Pawn.uasset

REM ── Actor components ────────────────────────────────────────
echo.
echo [Components] Copying actor components...
copy /Y "%SRC%\Blueprints\AC_PreCMCTick.uasset" "%DST%\Blueprints\" >nul && echo   OK: AC_PreCMCTick.uasset
copy /Y "%SRC%\Blueprints\AC_TraversalLogic.uasset" "%DST%\Blueprints\" >nul && echo   OK: AC_TraversalLogic.uasset
copy /Y "%SRC%\Blueprints\AC_VisualOverrideManager.uasset" "%DST%\Blueprints\" >nul && echo   OK: AC_VisualOverrideManager.uasset

REM ── Input (actions + mapping context) ───────────────────────
echo.
echo [Input] Copying input actions + mapping context...
if not exist "%DST%\Input" mkdir "%DST%\Input"
xcopy /Y /Q "%SRC%\Input\*.uasset" "%DST%\Input\" >nul
echo   OK: IMC_Sandbox + all IA_* input actions

REM ── Retargeted characters ───────────────────────────────────
echo.
echo [Characters] Copying retargeted character BPs...
if not exist "%DST%\Blueprints\RetargetedCharacters" mkdir "%DST%\Blueprints\RetargetedCharacters"
xcopy /Y /Q "%SRC%\Blueprints\RetargetedCharacters\*.uasset" "%DST%\Blueprints\RetargetedCharacters\" >nul
echo   OK: BP_Echo, BP_Manny, BP_Quinn, BP_Twinblast, BP_UE4_Mannequin, ABP_GenericRetarget

REM ── Characters (meshes, rigs, materials, textures) ──────────
echo.
echo [Characters] Copying character assets (meshes, rigs, textures)...
if not exist "%DST%\Characters" mkdir "%DST%\Characters"
xcopy /Y /S /Q /I "%SRC%\Characters\UE5_Mannequins" "%DST%\Characters\UE5_Mannequins" >nul
echo   OK: UE5_Mannequins (full tree)
xcopy /Y /S /Q /I "%SRC%\Characters\Echo" "%DST%\Characters\Echo" >nul
echo   OK: Echo (full tree)
xcopy /Y /S /Q /I "%SRC%\Characters\Paragon" "%DST%\Characters\Paragon" >nul
echo   OK: Paragon (full tree)
xcopy /Y /S /Q /I "%SRC%\Characters\UE4_Mannequin" "%DST%\Characters\UE4_Mannequin" >nul
echo   OK: UE4_Mannequin (full tree)
xcopy /Y /S /Q /I "%SRC%\Characters\UEFN_Mannequin" "%DST%\Characters\UEFN_Mannequin" >nul
echo   OK: UEFN_Mannequin (full tree)

REM ── MetaHumans ──────────────────────────────────────────────
echo.
echo [MetaHumans] Copying MetaHuman assets (Kellan + Common)...
if not exist "%DST%\MetaHumans" mkdir "%DST%\MetaHumans"
xcopy /Y /S /Q /I "%SRC%\MetaHumans\Common" "%DST%\MetaHumans\Common" >nul
echo   OK: MetaHumans/Common (full tree)
xcopy /Y /S /Q /I "%SRC%\MetaHumans\Kellan" "%DST%\MetaHumans\Kellan" >nul
echo   OK: MetaHumans/Kellan (full tree)

REM ── Data (structs, enums, curves, helpers) ──────────────────
echo.
echo [Data] Copying data assets (structs, enums, curves, helpers)...
if not exist "%DST%\Blueprints\Data" mkdir "%DST%\Blueprints\Data"
xcopy /Y /Q "%SRC%\Blueprints\Data\*.uasset" "%DST%\Blueprints\Data\" >nul
echo   OK: All data structs, enums, curves, BFL_HelpfulFunctions

REM ── Camera system ───────────────────────────────────────────
echo.
echo [Camera] Copying camera assets...
if not exist "%DST%\Blueprints\Cameras" mkdir "%DST%\Blueprints\Cameras"
xcopy /Y /Q "%SRC%\Blueprints\Cameras\*.uasset" "%DST%\Blueprints\Cameras\" >nul
echo   OK: CameraAsset, CameraDirector, CHT_CameraRig, E_CameraMode, E_CameraStyle

if not exist "%DST%\Blueprints\Cameras\Rigs" mkdir "%DST%\Blueprints\Cameras\Rigs"
xcopy /Y /Q "%SRC%\Blueprints\Cameras\Rigs\*.uasset" "%DST%\Blueprints\Cameras\Rigs\" >nul
echo   OK: All camera rigs (15 assets)

REM ── Movement modes ──────────────────────────────────────────
echo.
echo [Movement] Copying movement mode BPs...
if not exist "%DST%\Blueprints\MovementModes" mkdir "%DST%\Blueprints\MovementModes"
xcopy /Y /Q "%SRC%\Blueprints\MovementModes\*.uasset" "%DST%\Blueprints\MovementModes\" >nul
echo   OK: Walking, Falling, Slide + transitions

REM ── Anim modifiers + notifies ───────────────────────────────
echo.
echo [Animation] Copying anim modifiers + notifies...
if not exist "%DST%\Blueprints\AnimModifiers" mkdir "%DST%\Blueprints\AnimModifiers"
xcopy /Y /Q "%SRC%\Blueprints\AnimModifiers\*.uasset" "%DST%\Blueprints\AnimModifiers\" >nul
echo   OK: All anim modifiers (17 assets)

if not exist "%DST%\Blueprints\AnimNotifies" mkdir "%DST%\Blueprints\AnimNotifies"
xcopy /Y /Q "%SRC%\Blueprints\AnimNotifies\*.uasset" "%DST%\Blueprints\AnimNotifies\" >nul
echo   OK: All anim notifies (17 assets)

REM ── Control rig ─────────────────────────────────────────────
echo.
echo [ControlRig] Copying control rig...
if not exist "%DST%\Blueprints\ControlRigs" mkdir "%DST%\Blueprints\ControlRigs"
copy /Y "%SRC%\Blueprints\ControlRigs\CR_Biped_FootPlacement.uasset" "%DST%\Blueprints\ControlRigs\" >nul && echo   OK: CR_Biped_FootPlacement.uasset

REM ── Audio ───────────────────────────────────────────────────
echo.
echo [Audio] Copying audio assets...
if not exist "%DST%\Audio" mkdir "%DST%\Audio"
xcopy /Y /S /Q /I "%SRC%\Audio\Ambient" "%DST%\Audio\Ambient" >nul
echo   OK: Audio/Ambient (full tree)
xcopy /Y /S /Q /I "%SRC%\Audio\Foley" "%DST%\Audio\Foley" >nul
echo   OK: Audio/Foley (full tree)
xcopy /Y /S /Q /I "%SRC%\Audio\Mix" "%DST%\Audio\Mix" >nul
echo   OK: Audio/Mix (full tree)

REM ── Locomotor (Mech walker) ─────────────────────────────────
echo.
echo [Locomotor] Copying Locomotor assets...
if not exist "%DST%\Locomotor" mkdir "%DST%\Locomotor"
xcopy /Y /S /Q /I "%SRC%\Locomotor" "%DST%\Locomotor" >nul
echo   OK: Locomotor (full tree — BP_Walker, CR_Walker, Mech, etc.)

REM ── Level prototyping assets ────────────────────────────────
echo.
echo [Levels] Copying level prototyping assets...
if not exist "%DST%\Levels\LevelPrototyping" mkdir "%DST%\Levels\LevelPrototyping"
xcopy /Y /S /Q /I "%SRC%\Levels\LevelPrototyping" "%DST%\Levels\LevelPrototyping" >nul
echo   OK: LevelPrototyping (full tree — blocks, meshes, materials, textures)

REM ── Misc ────────────────────────────────────────────────────
echo.
echo [Misc] Copying misc assets...
if not exist "%DST%\Misc" mkdir "%DST%\Misc"
copy /Y "%SRC%\Misc\SandboxAnimCurveCompressionSettings.uasset" "%DST%\Misc\" >nul && echo   OK: SandboxAnimCurveCompressionSettings.uasset

REM ── Done ────────────────────────────────────────────────────
echo.
echo ============================================================
echo  RESTORE COMPLETE
echo ============================================================
echo.
echo  Next steps:
echo   1. Open TheWytching in Unreal Editor
echo   2. Let it re-import/fixup redirectors (may take a few minutes)
echo   3. Check Output Log for missing reference warnings
echo   4. Run lockdown.bat to commit everything to Git with LFS
echo.
echo  NOTE: Some assets may have broken references because they
echo  point to C++ classes from Wytcherly2's module, not TheWytching.
echo  GM_Sandbox and PC_Sandbox may need parent class re-parenting
echo  if the C++ class names differ between projects.
echo ============================================================
pause
