# TheWytching — Agent Context
### Aisling Brain System — Project Identity
`v1.0  Bootstrapped 2026-03-08  Crystallised from Agent_Context.md`

---

## Project Identity

- **Name:** TheWytching
- **Engine:** Unreal Engine 5.7, DX12, desktop-focused
- **Module:** `Source/TheWytching/`
- **Level:** `/Game/Levels/NPCLevel.NPCLevel`
- **Game mode:** `GM_Sandbox` → Player = `OllamaDronePawn`, Controller = `PC_Sandbox`
- **IDE:** JetBrains Rider
- **Source control:** Git
- **Brain path:** `[ProjectRoot]/Tools/Brain/`

---

## Prime Directive

Build a **Worker system** driven by a **Foreman AI**. Workers are interchangeable. The system defines *how* they work, not *what* they do. Workers wear hats — the task type defines the role. Get the coordination loop airtight first, then expand task types.

### The Loop
```
Foreman queries SmartObject subsystem → finds available slot matching needed task
  → finds idle Worker with matching capability tags
  → claims SmartObject slot on behalf of Worker
  → sends Worker command: "go to [SmartObject], interact"
  → Worker navigates → claims slot → BeginWork → TickWork → EndWork
  → Worker reports complete → slot released → loop restarts
```

---

## DO / DO NOT

- **DO** write all structural AI, controllers, data types in C++. Blueprint only for animations, designer-tweakable vars, visual debug.
- **DO** use `LogForeman` for Foreman C++ logs. `LogWytchAndroid` / `LogWytchWorker` for android/worker logs. Never `LogTemp`.
- **DO** use SmartObjects for work target discovery and slot claiming.
- **DO** use GameplayTags for worker capability matching (`Capability.*`).
- **DO** use `IWytch*` interfaces to define interaction behavior on target objects.
- **DO** use `IWytchCommandable` for Foreman-to-worker communication only.
- **DO** use `MarkSlotAsClaimed()` / `MarkSlotAsFree()`. NOT `Claim()` / `Release()`.
- **DO** use `FGameplayTagQuery::MakeQuery_MatchAnyTags()` for `FSmartObjectRequestFilter::ActivityRequirements`.
- **DO NOT** create Blueprint-only AI logic. All new AI code is C++.
- **DO NOT** use deprecated `USmartObjectSubsystem::Claim()` / `Release()`.
- **DO NOT** use tag-based actor queries (`GetAllActorsWithTag`) for workers or work targets (pre-Step-2 stubs only).
- **DO NOT** suggest Behavior Tree solutions. StateTree exclusively.
- **DO NOT** compile Blueprints during PIE. Stop PIE first.
- **DO NOT** expect Live Coding for new modules, plugins, or files with `GENERATED_BODY()`.
- **DO NOT** place `UForeman_BrainComponent` on the Character. Lives on `AForeman_AIController`.
- **DO NOT** create new .md files unless explicitly directed.
- **DO NOT** modify the Don't Touch list below without explicit instruction.

---

## Build Rules

- **Full compile (new files/modules):** Close editor → build from Rider → reopen editor
- **Incremental compile (existing files only):** Live Coding in-editor (Ctrl+Alt+F11)
- **Blueprint compile:** In-editor, never during PIE
- **Include convention:** Path-relative includes (`#include "Foreman/ForemanTypes.h"`)
- **Key deps (Build.cs):** `HTTP, Json, JsonUtilities, ImageWrapper, AIModule, NavigationSystem, GameplayAbilities, GameplayTags, GameplayTasks, StateTreeModule, GameplayStateTreeModule, SmartObjectsModule`

---

## Don't Touch (Stable)

- `OllamaDronePawn` — player pawn
- `CognitiveMapJsonLibrary` — JSON utility, separate concern
- `GM_Sandbox` / `PC_Sandbox` — game mode and player controller
- `BT_Foreman` / `BB_Foreman` — archived reference only
- Widget assets (`WBP_CommandInput`, `WBP_CognitiveDisplay`, `WBP_CognitiveEntry`) — intentionally disabled

---

## Three-Layer Architecture

| Layer | Mechanism | Handles |
|---|---|---|
| SmartObjects | `USmartObjectSubsystem` | Discovery, spatial queries, slot claiming/releasing |
| IWytch* Interfaces | C++ UInterface | What happens during interaction |
| Capability Tags | GameplayTags on workers | Worker↔slot matching |

**Foreman-mediated claiming:** Foreman queries subsystem, picks worker-slot pair, sends worker the claim handle. Worker holds the actual claim. Non-standard UE5 pattern — intentional, preserves command hierarchy.

---

## Current Source State (verified 2026-03-08)

All compiled ✓:

| File | Purpose | State |
|---|---|---|
| `ForemanTypes.h/.cpp` | `LogForeman` | Stable |
| `AndroidTypes.h/.cpp` | Enums + `LogWytchAndroid` + `LogWytchWorker` | Stable |
| `AndroidConditionComponent.h/.cpp` | Power, subsystems, degradation | Stable |
| `IWytchInteractable.h/.cpp` | Base interactable UInterface | Stable |
| `IWytchCarryable.h/.cpp` | Pickable/movable UInterface | Stable |
| `IWytchWorkSite.h/.cpp` | Work location UInterface (`BeginWork/TickWork/EndWork`) | Stable |
| `IWytchCommandable.h/.cpp` | Foreman→worker command UInterface | Stable |
| `Foreman_AIController.h/.cpp` | StateTreeAI + BrainComponent + Perception; FObjectFinder wires ST_Foreman | Active — Step 2 target |
| `ForemanStateTreeTasks.h/.cpp` | PlanJob, AssignWorker, Monitor, Rally, Wait | Active — tag-based stubs, Step 2 target |
| `ForemanStateTreeConditions.h/.cpp` | HasIdleWorkers, HasAvailableWork | Active — tag-based stubs, Step 2 target |
| `ForemanStateTreeEvaluators.h/.cpp` | WorkAvailability (0.5s timer) | Active — tag-based stub, Step 2 target |
| `ForemanSurveyComponent.h/.cpp` | Secondary discovery for non-SmartObject items | Stable, reduced scope |
| `Foreman_BrainComponent.h/.cpp` | LLM/vision cognitive pipeline | Stable |
| `Foreman_Character.h/.cpp` | Future C++ base class (not active) | Future |
| `CognitiveMapJsonLibrary.h/.cpp` | JSON utility | Stable — don't touch |
| `OllamaDronePawn.h/.cpp` | Player pawn | Stable — don't touch |
| `PC_Wytching.h/.cpp` | Player controller | Stable |
| `PatrolPoint.h/.cpp` | Patrol waypoint | Stable |

**Content assets:**
- `/Game/Foreman/SmartObjects/SOD_Build` — SmartObjectDefinition, `Task.Build`, 1 slot ✓
- `/Game/Foreman/BP_WorkStation` — Actor Blueprint, SmartObjectComponent, `IWytchWorkSite` ✓
- `/Game/Foreman/ST_Foreman` — StateTree: Root→Idle→Dispatch(Plan+Assign)→Monitor ✓
- `WorkStation_Build_01` placed in NPCLevel ✓
- `DefaultGameplayTags.ini` — 31 tags: `Capability.*`, `Task.*`, `Status.*`, `Mode.*` ✓

---

## Known Gotchas

- **Blueprint BeginPlay trap:** Child with `Event BeginPlay` node (even disconnected) overrides parent's BeginPlay entirely. Caught and fixed 2026-02-25.
- **Dead BT nodes in BP_FOREMAN_V2:** Remove during cleanup pass.
- **SmartObject + Foreman-mediated pattern:** Foreman queries, assigns. Workers do NOT autonomously find SmartObjects.
- **SmartObject API:** `MarkSlotAsClaimed()` / `MarkSlotAsFree()` — NOT `Claim()` / `Release()`. `ActivityRequirements` is `FGameplayTagQuery` not `FGameplayTagContainer`.
- **Linked StateTree bugs (5.5+):** Global tasks with params crash hard. `ShouldStateChangeOnReselect` ignored intermittently. AI Debugger crashes viewing linked assets (UE-273333). Use `On State Completed` not `On Tick` for transitions in linked assets.
- **`BP_FOREMAN_V2` stays Blueprint:** Thin shell by design. C++ AIController drives all logic. Not debt.

---

*TheWytching Agent Context — Aisling Brain System — Bootstrapped 2026-03-08*

