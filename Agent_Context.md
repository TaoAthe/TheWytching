# TheWytching: Agent Context

**Last updated:** 2026-02-28 (SmartObject foundation validated — Step 1 complete)

---

## Prime Directive

Build a robust, generic **Worker system** driven by a **Foreman AI**. Workers are interchangeable — the system defines *how* they work, not *what* they do. Workers wear hats — the task type (build, transport, demolition, etc.) defines the role. Get the coordination loop airtight first, then expand task types.

### The Loop (Runtime Flow)

```
Foreman queries SmartObject subsystem → finds available SmartObject slot matching needed task
  → finds idle Worker with matching capability tags
  → claims SmartObject slot on behalf of Worker
  → sends Worker command: "go to [SmartObject], interact"
  → Worker navigates to SmartObject
  → Worker interacts (behavior defined by IWytch* interface on the target)
  → Worker reports complete
  → SmartObject slot released → loop restarts
```

**Three layers work together:**
- **SmartObjects** — discovery, spatial queries, slot claiming/releasing (engine-native)
- **IWytch\* interfaces** — define *what happens* during interaction (custom behavior)
- **Capability tags** — match workers to tasks they can perform (GameplayTags on workers)

Everything in this doc exists to make this loop work reliably with any number of workers, any worker type, and any task type hat.

---

## DO / DO NOT

- **DO** write all structural AI, controllers, and data types in C++. Blueprint only for animations, designer-tweakable vars, visual debug. (BP_FOREMAN_V2 is a thin Blueprint shell — C++ AIController + StateTree drive all logic. This is intentional, not debt.)
- **DO** use `LogForeman` category for all Foreman C++ logs.
- **DO** use SmartObjects for work target discovery and slot claiming. Do not build custom discovery/claim systems.
- **DO** use GameplayTags for worker capability matching (e.g. `Capability.Hauling`, `Capability.LaserCutter`, `Capability.Combat`).
- **DO** use `IWytch*` interfaces to define interaction behavior on target objects. SmartObjects gets the worker there and claims the slot; the interface defines what happens.
- **DO** use `IWytchCommandable` for Foreman-to-worker communication. `IWytch*` interfaces are for worker-to-object interaction. `IWytchCommandable` is for Foreman-to-worker commands.
- **DO** use `LogWytchAndroid` for android condition/degradation logging, `LogWytchWorker` for worker-specific logging. `LogForeman` remains for Foreman dispatch logic.
- **DO** use explicit `Print String` traces for runtime validation. Transition to Visual Logger (`vlog`) + `LogForeman`/`LogWytchWorker` categories after Step 5 (Print Strings become noise with multiple agents).
- **DO** route new Foreman behavior to `AIC_Foreman` or new StateTree tasks. Not `Foreman_Character.h` (future base class, not active).
- **DO** use `MarkSlotAsClaimed()` / `MarkSlotAsFree()` for SmartObject claims. NOT the deprecated `Claim()` / `Release()` wrappers.
- **DO NOT** create new Blueprint-only AI logic. All new AI code is C++.
- **DO NOT** build custom world-scanning or claim/release logic. SmartObject subsystem handles this now.
- **DO NOT** use deprecated `USmartObjectSubsystem::Claim()` / `Release()`. Use `MarkSlotAsClaimed()` / `MarkSlotAsFree()`.
- **DO NOT** use `FGameplayTagContainer` for `FSmartObjectRequestFilter::ActivityRequirements` — it requires `FGameplayTagQuery` (build with `FGameplayTagQuery::MakeQuery_MatchAnyTags()`).
- **DO NOT** use tag-based actor queries (`GetAllActorsWithTag`) for finding workers or work targets.
- **DO NOT** re-add or auto-generate interface function graphs that already exist (causes `"Graph named 'X' already exists"` errors).
- **DO NOT** suggest Behavior Tree solutions. Project uses StateTree exclusively. `BT_Foreman` is archived reference only.
- **DO NOT** compile Blueprints during PIE. Stop PIE first.
- **DO NOT** expect Live Coding to handle new modules, plugins, or files with `GENERATED_BODY()`. These require full editor restart.
- **DO NOT** place `UForeman_BrainComponent` on the Character. It lives on `AForeman_AIController`.
- **DO NOT** modify anything in the Don't Touch list (below) without explicit developer instruction.

---

## Build & Source Control

- **Source control:** Git
- **Full compile (new files/modules):** Close editor → build from IDE (JetBrains Rider) → reopen editor
- **Incremental compile (existing files only):** Live Coding in-editor (Ctrl+Alt+F11)
- **Blueprint compile:** In-editor, but **never during PIE**
- **Include convention:** Use path-relative includes (`#include "Foreman/ForemanTypes.h"`), not engine-wide (`#include "Engine/Engine.h"`)
- **Log convention:** `UE_LOG(LogForeman, Log, TEXT("..."));` — declared in `ForemanTypes.h`. `LogWytchAndroid` / `LogWytchWorker` declared in `AndroidTypes.h`. Never use `LogTemp` in project code.

---

## Don't Touch (Stable — Do Not Modify)

These are validated and working. Do not change without explicit instruction:

- `OllamaDronePawn` — player pawn, not part of worker/foreman system
- `CognitiveMapJsonLibrary` — JSON cognitive map utility, separate concern
- `GM_Sandbox` / `PC_Sandbox` — game mode and player controller, stable
- `BT_Foreman` / `BB_Foreman` — archived reference only. Do not reactivate or modify
- Widget assets (`WBP_CommandInput`, `WBP_CognitiveDisplay`, `WBP_CognitiveEntry`) — intentionally disabled

> Note: `ST_Foreman` was previously on this list but will be modified during SmartObjects integration. It is now an active development target.

---

## Engine & Stack

- **Unreal Engine 5.7**, DX12, desktop-focused
- **C++ module:** `Source/TheWytching/`
- **Key deps (Build.cs):** HTTP, Json, JsonUtilities, ImageWrapper, AIModule, NavigationSystem, GameplayAbilities, GameplayTags, GameplayTasks, StateTreeModule, GameplayStateTreeModule, SmartObjectsModule
- **Key plugins:** StateTree, SmartObjects, GameplayInteractions, GameplayBehaviors, Mover, MetaHuman, Chooser, PoseSearch, MotionWarping
- **Level:** `/Game/Levels/NPCLevel.NPCLevel`
- **Game mode:** `GM_Sandbox` → Player = `OllamaDronePawn`, Controller = `PC_Sandbox`

---

## Architecture: Three-Layer System

### Layer 1: SmartObjects (Discovery + Claiming)

SmartObjects replace all custom discovery and claim/release logic. This is the engine-native layer.

**How it works in this project:**
- Every work target (WorkStation, Block, quarry face, defend point) has a `USmartObjectComponent` with one or more slots
- Each slot is defined by a `USmartObjectDefinition` asset with GameplayTags describing the task type (e.g. `Task.Build`, `Task.Transport`, `Task.Demolition`)
- Slots can have **capability requirements** via tags (e.g. a demolition slot requires `Capability.LaserCutter` — only AutoBots qualify)
- The **Foreman's StateTree** queries the SmartObject subsystem to find available slots, filtered by task type and worker capability
- Claiming is handled by the SmartObject subsystem — no custom `TryClaim`/`Release` needed

**Foreman-mediated claiming:** The Foreman queries available slots, picks the best worker-slot pair, then instructs the worker to claim and interact. Workers do **not** autonomously find SmartObjects — the Foreman is always the coordinator. This is non-standard (typical UE5 has individual agents find their own SmartObjects) but preserves the command hierarchy.

**Claim ownership:** The **worker** holds the claim, not the Foreman. The Foreman identifies the slot and sends the worker a claim handle. The worker claims on arrival. If the worker fails or times out, the slot is released automatically.

**UE 5.7 SmartObject API (verified from engine source):**
- `USmartObjectSubsystem::GetCurrent(World)` — static accessor for the subsystem
- `FindSmartObjects(FSmartObjectRequest, TArray<FSmartObjectRequestResult>&, FConstStructView)` — spatial query
- `FSmartObjectRequestFilter::ActivityRequirements` is `FGameplayTagQuery` — use `FGameplayTagQuery::MakeQuery_MatchAnyTags(TagContainer)` to build
- `MarkSlotAsClaimed(SlotHandle, ClaimPriority, UserData)` → returns `FSmartObjectClaimHandle`
- `MarkSlotAsFree(ClaimHandle)` — releases the claim
- `GetSlotTransform(RequestResult)` → `TOptional<FTransform>` for navigation targeting
- ⚠️ `Claim()` and `Release()` are **deprecated wrappers** — do not use

**SmartObject Definitions (tag-based, not one-per-type):**
- Asset path: `/Game/Foreman/SmartObjects/` — naming convention: `SOD_[TaskType]` (e.g. `SOD_Build`, `SOD_Transport`, `SOD_Demolition`)
- One `USmartObjectDefinition` asset per task type, with slot tags for capability requirements
- Adding a new hat = new definition asset + new tag values, minimal code change
- Keep definitions minimal — the SmartObject defines *where* and *who can*, the `IWytch*` interface defines *what happens*

### Layer 2: IWytch* Interfaces (Interaction Behavior)

SmartObjects get the worker to the right place and handle the slot. `IWytch*` interfaces define what the worker actually *does* when it arrives.

| Interface | Inherits | Purpose |
|---|---|---|
| `IWytchInteractable` | — | Base: anything in the world that can be interacted with |
| `IWytchCarryable` | `IWytchInteractable` | Pickable/movable objects (blocks, resources, items) |
| `IWytchWorkSite` | `IWytchInteractable` | Work locations — `BeginWork`/`TickWork`/`EndWork` contract |
| `IWytchCommandable` | — | Foreman→worker commands: `ReceiveSmartObjectAssignment`, `GetWorkerState`, `GetCapabilities`, `AbortCurrentTask` |

All functions are `BlueprintNativeEvent` + `BlueprintCallable`. C++ provides defaults, Blueprints can override.

**Interface status:**
- `IWytchInteractable` — Compiled ✓
- `IWytchCarryable` — Compiled ✓
- `IWytchWorkSite` — Updated for SmartObjects (old `TryClaim`/`ReleaseSite`/`GetClaimant`/`GetWorkType` removed; new `BeginWork`/`TickWork`/`EndWork`/`GetTaskType`/`IsOperational`/`GetInteractionDuration` added). Pending compile.
- `IWytchCommandable` — New. Pending compile.

**Separation of concerns:** `IWytch*` interfaces (Interactable, Carryable, WorkSite) define what **objects** do when interacted with. `IWytchCommandable` defines what **workers** do when commanded by the Foreman. Objects are interactable, workers are commandable.

**The handoff:** SmartObject slot is claimed → worker arrives → worker calls `IWytchWorkSite::BeginWork()` on the target → `TickWork()` each frame → `EndWork()` on completion/abort/failure. The interface is the "hat behavior" — same worker, different interaction depending on the target's interface.

⚠️ If `UWytchCarryable : public UWytchInteractable` causes UHT issues in future, flatten to independent interfaces.

### Layer 3: Worker Capabilities (GameplayTags)

Workers carry GameplayTags describing what they can do. The Foreman uses these to match workers to SmartObject slots.

**Tag declaration:** All project GameplayTags declared in `Config/DefaultGameplayTags.ini`. Four hierarchies:
- `Task.*` — on SmartObject slots, describes what kind of work (`Task.Build`, `Task.Transport`, `Task.Demolition`, `Task.Repair`, `Task.Recharge`, `Task.SelfMaintenance`, `Task.Recreation`, `Task.Idle`)
- `Capability.*` — on workers, describes what they can do (`Capability.Hauling`, `Capability.Building`, `Capability.LaserCutter`, `Capability.Demolition`, `Capability.Defense`, `Capability.Combat`, `Capability.Patrol`)
- `Status.*` — condition state tags for StateTree conditions (`Status.Power.*`, `Status.NeedsRepair`, `Status.CommLink.Lost`, `Status.Vision.*`, `Status.Locomotion.*`)
- `Mode.*` — duty cycle tags (`Mode.OnDuty`, `Mode.OffDuty`, `Mode.Maintenance`, `Mode.Disabled`)

| Worker Type | Capability Tags | Role |
|---|---|---|
| General Worker (unified class) | `Capability.Hauling`, `Capability.Building` | Standard labor — carry, build, basic tasks |
| AutoBot | `Capability.LaserCutter`, `Capability.Demolition`, `Capability.Defense` | Specialist — laser cutting, demolition, guard duty. Cannons on legs. |
| Soldier Bot (future) | `Capability.Combat`, `Capability.Patrol` | Military — combat, patrol, area denial |

**Matching flow:** Foreman queries SmartObject subsystem for available slots → checks slot's required capability tags → finds idle worker whose capability tags satisfy the requirement → assigns.

Workers that lack the required capability are never assigned to that slot. AutoBots don't get sent to haul blocks. General workers don't get sent to demolition sites. The tag system handles this filtering naturally.

---

## Foreman (The Boss)

- **Active actor:** `BP_FOREMAN_V2` (Blueprint Character shell — AI-controlled, not player-possessed. Stays as BP; C++ AIController drives all logic. This is not migration debt.)
- **AI Controller:** `AIC_Foreman` (inherits `AIC_NPC_SmartObject` — project base AIController class that predates this pivot; already has SmartObject support baked in, which is why this integration is natural rather than a new dependency)
  - `StateTreeAIComponent` → drives `ST_Foreman` (`bStartLogicAutomatically=True`)
  - `UAIPerceptionComponent` — sight: radius 3000, lose 3500, peripheral 90°
  - `UForeman_BrainComponent` — LLM/vision cognitive pipeline (local endpoint `localhost:1234`)
- **StateTree:** `/Game/Foreman/ST_Foreman` — 6 states (Root, Idle, Dispatch [group], Plan, Assign, Monitor), 4 tasks, 6 transitions, 2 enter conditions, 1 evaluator. Idle loop validated in PIE. Dispatch conditions currently check actor tags (pre-SmartObject stubs).
- **StateTree wiring:** `DefaultStateTree` set via `ConstructorHelpers::FObjectFinder` in `AForeman_AIController` constructor. `BeginPlay` calls `SetStateTree()` + `StartLogic()` retry (OnPossess fires before SetStateTree during `Super::BeginPlay()`).
- **Gait:** Walks by default. `bForceRun` bool on BP_FOREMAN_V2 for StateTree tasks needing urgency.

**If StateTree doesn't start:** Check AIC_Foreman BeginPlay chain (child must call `Parent: Event BeginPlay` or have no BeginPlay node). Verify `bStartLogicAutomatically=True` on `StateTreeAIComponent`. Verify component references `ST_Foreman`, not another StateTree asset. Check Output Log for `LogStateTree` errors.

### Foreman StateTree Tasks (C++)

⚠️ `MIGRATION:` These tasks will be refactored for SmartObjects. Current tag-based queries are being replaced.

| Task | Current Behavior | SmartObjects Target |
|---|---|---|
| `FForemanTask_PlanJob` | Scans world with tags, scores by distance | Query SmartObject subsystem for available slots, match to idle workers by capability |
| `FForemanTask_AssignWorker` | Custom TryClaim, sends Build command | Send worker claim handle + SmartObject reference, worker claims slot |
| `FForemanTask_Monitor` | Oversight tick (3s), checks active assignments | Monitor active SmartObject claims, detect completion/failure/timeout |
| `FForemanTask_Rally` | Calls all workers to Foreman's position | Unchanged — not SmartObject-related |
| `FForemanTask_Wait` | Idles for duration, optional LLM scan | Unchanged |

### Foreman StateTree Conditions & Evaluator (C++)

| Name | Current | SmartObjects Target |
|---|---|---|
| `FForemanCondition_HasIdleWorkers` | Checks "Idle" tag on actors | Query registered workers for idle state (interface-based) |
| `FForemanCondition_HasAvailableWork` | Checks "WorkStation" tag without "Claimed" | Query SmartObject subsystem for unclaimed slots |
| `FForemanEval_WorkAvailability` | Tag-based scan every 0.5s | SmartObject subsystem query every 0.5s |

---

## Worker System

### Current State (Pre-Unification — delete after unification)

Verify counts in-editor — may have changed since last update.

| Class | Status | Notes |
|---|---|---|
| `BP_AutoBot` | Partial | Implements `BPI_WorkerCommand` (GetWorkerState, GetCurrentTask). **No ReceiveCommand handler** — auto-wanders only. Future: specialist with `Capability.LaserCutter` etc. |
| `SandboxCharacter_Mover` | Partial | Has `ReceiveCommand_Impl` but does **not formally implement `BPI_WorkerCommand`** (function exists without interface binding). Intentional — will be replaced during worker unification. Do not add interface binding. |
| `SandboxCharacter_CMC` | Legacy | Tagged "Legacy". Not workers. Ignore |

**Worker state enum:** `EWorkerState` in `AndroidTypes.h` (C++) — Idle, MovingToTask, Working, Returning, Unavailable. Blueprint enum `/Game/Foreman/E_WorkerState` is deprecated.

**Worker command interface:** `IWytchCommandable` (C++) — `ReceiveSmartObjectAssignment()`, `GetWorkerState()`, `GetCapabilities()`, `AbortCurrentTask()`. Replaces `BPI_WorkerCommand` Blueprint interface.

**Worker roster (Step 2):** Option A — registration on BeginPlay. Workers call `Foreman->RegisterWorker(this)`, Foreman maintains `TArray<TWeakObjectPtr<AActor>> RegisteredWorkers`.

### 🎯 Target: Unified Worker Architecture

**Two worker classes**, not one — because specialists and generalists have fundamentally different physical capabilities:

**General Worker (new unified class)**
- Implements `IWytchCommandable` (Foreman sends commands) + `IWytchInteractable` (so the Foreman can interact with it if needed)
- Has `UAndroidConditionComponent` — power, subsystem health, capability recalculation
- Carries `FGameplayTagContainer` with capability tags (`Capability.Hauling`, `Capability.Building`)
- State machine: Idle → Commanded → Navigating → Interacting → Returning → Idle
- Receives commands from Foreman via `IWytchCommandable::ReceiveSmartObjectAssignment()`
- Generic interaction: calls `IWytchWorkSite::BeginWork/TickWork/EndWork` on target

**AutoBot (refactored from existing BP_AutoBot)**
- Same architecture as General Worker but with different capability tags (`Capability.LaserCutter`, `Capability.Demolition`, `Capability.Defense`)
- Idle behavior: auto-wander/patrol (existing behavior preserved)
- Specialist interactions: laser-cut quarry face → spawns `IWytchCarryable` blocks for general workers to haul
- Can be assigned to `IWytchDefendPoint` (future) for guard duty

**Both worker types share:**
- Same `IWytchCommandable` contract (Foreman queries/commands identically)
- Same `UAndroidConditionComponent` (power, subsystems, degradation)
- Same SmartObject interaction pattern (receive assignment → navigate → claim → BeginWork → TickWork → EndWork → release)
- Different capability tags (determines which slots they're eligible for)

---

## ForemanSurveyComponent

- **Lives on:** `AForeman_AIController` (created as subobject)
- **Original purpose:** `DiscoverInteractables(float SearchRadius)` — interface-based world scanning
- **Current role (repurposed):** Secondary scan for **non-SmartObject discovery** — environmental objects, loose items, dynamically spawned carryables that haven't registered as SmartObjects yet. No longer the primary discovery mechanism — SmartObject subsystem handles that.
- **Do not delete** — utility still needed for edge cases.

---

## C++ Source Reference

| File | Purpose | Status |
|---|---|---|
| `ForemanTypes.h/.cpp` | `LogForeman` log category, shared types | Stable |
| `Foreman_AIController.h/.cpp` | AIController: StateTree + Perception + BrainComponent + SurveyComponent | Active — needs `GetOwnCondition()`, worker roster (Step 2) |
| `Foreman_BrainComponent.h/.cpp` | LLM/vision cognitive pipeline | Stable |
| `Foreman_Character.h/.cpp` | Base C++ character with GAS — **future base class**, not on level. Do not modify unless building a C++ Foreman character. | Future base |
| `ForemanStateTreeTasks.h/.cpp` | 5 StateTree tasks | Active — refactoring for SmartObjects |
| `ForemanStateTreeConditions.h/.cpp` | 2 StateTree conditions | Active — refactoring for SmartObjects |
| `ForemanStateTreeEvaluators.h/.cpp` | 1 StateTree evaluator | Active — refactoring for SmartObjects |
| `ForemanSurveyComponent.h/.cpp` | Secondary discovery for non-SmartObject interactables | Stable, reduced scope |
| `IWytchInteractable.h/.cpp` | Base UInterface — anything interactable | Compiled ✓ |
| `IWytchCarryable.h/.cpp` | UInterface — pickable/movable objects | Compiled ✓ |
| `IWytchWorkSite.h/.cpp` | UInterface — work locations (`BeginWork`/`TickWork`/`EndWork` contract) | Compiled ✓ |
| `IWytchCommandable.h/.cpp` | UInterface — Foreman→worker commands (`ReceiveSmartObjectAssignment`, `GetWorkerState`, `GetCapabilities`, `AbortCurrentTask`) | Compiled ✓ |
| `AndroidTypes.h/.cpp` | Enums (`EWorkerState`, `EAndroidPowerState`, `ESubsystemStatus`, `ESubsystemType`, `EAndroidReadiness`, `EAbortReason`, `EWorkEndReason`), `LogWytchAndroid` + `LogWytchWorker` log categories | Compiled ✓ |
| `AndroidConditionComponent.h/.cpp` | `UAndroidConditionComponent` — power, structural HP, subsystem health, capability recalculation, personality seed | Compiled ✓ |
| `CognitiveMapJsonLibrary.h/.cpp` | JSON read/write for cognitive map | Stable — don't touch |
| `OllamaDebugActor.h/.cpp` | Debug LLM actor | Stable |
| `OllamaDronePawn.h/.cpp` | Player pawn | Stable — don't touch |
| `PC_Wytching.h/.cpp` | Player controller (C++) | Stable |
| `PatrolPoint.h/.cpp` | Patrol waypoint actor | Stable |

---

## Key Design Decisions

1. **StateTree over BT** — Foreman is state-based (Dispatch → Monitor → Rally → Idle). StateTrees model this naturally. BT archived.
2. **SmartObjects for discovery + claiming** — Engine-native spatial queries and slot management. No custom scanning or claim/release logic. SmartObject definitions use GameplayTags for task type and capability requirements.
3. **IWytch\* interfaces for behavior** — SmartObjects handle *where* and *who*. Interfaces handle *what happens*. Clean separation.
4. **Foreman-mediated assignment** — Workers do not autonomously find SmartObjects. The Foreman is always the coordinator. Non-standard UE5 pattern, but preserves command hierarchy.
5. **Worker claims the slot, not the Foreman** — Foreman identifies the match, worker holds the actual claim. Prevents Foreman from becoming a bottleneck.
6. **Capability tags for specialization** — GameplayTags on workers (`Capability.Hauling`, `Capability.LaserCutter`, etc.) matched against SmartObject slot requirements. Adding a new worker type = new tag set, not new code.
7. **BrainComponent on Controller, not Character** — Controller persists across repossession.
8. **BP_FOREMAN_V2 stays as Blueprint** — Thin shell. All logic in C++ AIController + StateTree. Not debt.
9. **Timer-based dispatch (not Tick)** — Lower noise, easier debugging.
10. **Worker state as source of truth** — Foreman queries `GetWorkerState()`, never infers from movement.
11. **Generic worker commands** — Workers get told "go to SmartObject X, interact." The target's `IWytch*` interface defines behavior. Workers wear hats — the task is the hat.

---

## Known Gotchas

- **Blueprint BeginPlay inheritance trap:** A child class with an `Event BeginPlay` node (even disconnected) **overrides parent's BeginPlay entirely**. Parent logic only runs if child calls `Parent: Event BeginPlay`. This silently broke StateTree startup — caught 2026-02-25.
- **Dead BT nodes in BP_FOREMAN_V2:** `Get AIController` + `Run Behavior Tree` nodes in EventGraph from pre-StateTree era. Remove during cleanup pass.
- **Widget path disabled:** Old command/cognitive widget flow intentionally detached. Keep disabled until replacement designed.
- **Foreman_BrainComponent perception fallback:** Auto-detects controller's perception or creates its own. Verify after runtime test that it finds the controller's component.
- **SmartObject + Foreman-mediated pattern:** Most UE5 tutorials show individual agents finding their own SmartObjects. This project intentionally does NOT do that. The Foreman queries the subsystem and assigns workers. If you see examples of agents with `FindAndClaimSmartObject` in their own StateTree, that's not our pattern.
- **SmartObject subsystem must be active:** Verify `USmartObjectSubsystem` is available at runtime. SmartObjects plugin must be enabled (it is). If queries return nothing, check that SmartObject components are registered with the subsystem (happens automatically on BeginPlay, but verify).
- **Linked StateTree bugs (UE 5.5+):** Global tasks with parameters in linked assets crash hard. `ShouldStateChangeOnReselect` may be ignored intermittently. AI Debugger crashes when viewing linked assets (UE-273333). Linked assets transitioning via On Tick can loop infinitely (UE-222632) — use On State Completed instead. Test linked assets in isolation before integrating. Fall back to duplication if unstable.
- **SmartObject API naming:** `MarkSlotAsClaimed()` / `MarkSlotAsFree()` are the current 5.7 API. `Claim()` / `Release()` are deprecated wrappers. `FSmartObjectRequestFilter::ActivityRequirements` is a `FGameplayTagQuery`, not a `FGameplayTagContainer` — build with `FGameplayTagQuery::MakeQuery_MatchAnyTags()`.
- **IWytchWorkSite interface updated:** Old `TryClaim`/`ReleaseSite`/`GetClaimant`/`GetWorkType` removed (2026-02-27). BP_WorkStation Blueprint overrides of these old functions will show as orphaned and need deleting in the editor.

---

## Roadmap

### Completed ✓
- Worker registration/state contract (Blueprint)
- Foreman assignment loop (Blueprint, runtime-validated)
- WorkStation foundation (Blueprint, runtime-validated)
- C++ StateTree architecture (Phases 0–4: types, tasks, conditions, evaluator, ST_Foreman asset, integration — all validated)
- AIC_Foreman startup fixes (orphaned BeginPlay, wrong StateTree ref, broken RunStateTree call)
- Foreman gait override (walks by default, `bForceRun` for urgency)
- IWytch* C++ interfaces (compiled and working)

### Active: SmartObjects Integration + Worker Unification

Steps are sequential unless noted:

1. **SmartObject foundation** — Add `USmartObjectComponent` to `BP_WorkStation`. Create `USmartObjectDefinition` asset with `Task.Build` tag. Verify registration with `USmartObjectSubsystem` at runtime. *No dependencies*
2. **Foreman SmartObject queries** — Refactor `FForemanTask_PlanJob` to query `USmartObjectSubsystem` for available slots instead of tag-based scanning. Refactor `FForemanCondition_HasAvailableWork` similarly. *Depends on: step 1*
3. **Worker SmartObject interaction** — Add SmartObject claim/use capability to workers. Worker receives claim handle from Foreman, claims slot on arrival, releases on completion/failure. *Depends on: step 2*
4. **Capability tag system** — Define `GameplayTag` hierarchy under `Capability.*`. Tag General Workers and AutoBots with their respective capabilities. Add capability filtering to Foreman's SmartObject queries. *Can begin design in parallel with steps 1–3, implementation depends on step 3*
5. **Unify General Worker class** — Single robust worker class: state machine, SmartObject interaction, `IWytch*` interface calls, capability tags. Replaces both `SandboxCharacter_Mover` and the general-purpose aspects of `BP_AutoBot`. *Depends on: steps 3 + 4*
6. **Refactor AutoBot as specialist** — Keep existing class, add capability tags (`Capability.LaserCutter`, `Capability.Demolition`, `Capability.Defense`), add SmartObject interaction, add `ReceiveCommand` handler. Idle = patrol/wander. *Depends on: steps 3 + 4, can parallel with step 5*
7. **Transport demo** — Add `Task.Transport` SmartObject definition. Create `BP_Block` (`IWytchCarryable`), transport pickup/dropoff SmartObjects. Foreman dispatches general workers to carry blocks A→B. *Depends on: step 5*
8. **Quarry demo** — Add `Task.Demolition` SmartObject definition on a quarry face. AutoBot laser-cuts blocks (spawns `IWytchCarryable` actors). General workers haul the blocks. Two-stage pipeline validated. *Depends on: steps 6 + 7*
9. **End-to-end test** — Full loop: AutoBots cut blocks → General Workers haul → WorkSites receive. Foreman orchestrates both worker types simultaneously via SmartObject queries and capability matching. *Depends on: all above*

### Future
- `FForemanTask_SourceMaterials` (when resource system exists)
- Soldier Bot class (`Capability.Combat`, `Capability.Patrol`)
- `IWytchDefendPoint` interface for guard/defense SmartObjects
- Redesigned cognitive display widget
- Replace all debug prints with structured `LogForeman` exclusively
- Additional task type hats (harvest, craft, repair, etc.)

---

## Deprecated / Being Replaced

These systems are still in the codebase but are being superseded by SmartObjects:

| System | Replaced By | When to Remove |
|---|---|---|
| Tag-based actor queries in StateTree tasks/conditions | SmartObject subsystem queries | After roadmap step 2 is validated |
| `ForemanSurveyComponent::DiscoverInteractables` (as primary discovery) | SmartObject subsystem queries | Repurposed, not removed — secondary scan role |
| `BPI_WorkerCommand` / `BPI_WorkTarget` Blueprint interfaces | `IWytchCommandable` C++ interface + SmartObject interaction | After worker unification (step 5) |
| `BP_WorkStation` self-registration with Foreman | SmartObject auto-registration with subsystem | After roadmap step 1 is validated |
| `/Game/Foreman/E_WorkerState` Blueprint enum | `EWorkerState` in `AndroidTypes.h` | After worker unification (step 5) |

**Already removed:**
- `IWytchWorkSite::TryClaim()` / `ReleaseSite()` / `GetClaimant()` / `GetWorkType()` — replaced by SmartObject claiming + `BeginWork`/`TickWork`/`EndWork`/`GetTaskType` contract (2026-02-27)

---

## Immediate Next Step (Start Here)

> This section is ephemeral. Check items off as completed, then replace with the next active task.

- [x] Restart Unreal Editor — IWytch* interfaces compiled ✓
- [x] Created `AndroidTypes.h/.cpp` — all enums + log categories
- [x] Created `AndroidConditionComponent.h/.cpp` — full degradation component
- [x] Created `IWytchCommandable.h/.cpp` — Foreman→worker command interface
- [x] Updated `IWytchWorkSite.h` — removed old claim functions, added BeginWork/TickWork/EndWork contract
- [x] Updated `DefaultGameplayTags.ini` — 31 new tags (Capability, Task, Status, Mode hierarchies)
- [x] Added `SmartObjectsModule` to `Build.cs`

**~~⚠️ BLOCKING: Close editor → Build from Rider → Reopen editor~~** ✓ Done 2026-02-28
~~New files with `GENERATED_BODY()` + `Build.cs` module change = full restart required, no Live Coding.~~
After compile:
- [x] Verify all new files compile cleanly ✓
- [ ] Delete orphaned BP_WorkStation Blueprint overrides (TryClaim, ReleaseSite, GetClaimant, GetWorkType) — N/A (BP_WorkStation created fresh)

**Roadmap Step 1: SmartObject Foundation ✓ (completed 2026-02-28)**
- [x] Created `/Game/Foreman/SmartObjects/SOD_Build` — SmartObjectDefinition with `Task.Build` activity tag, 1 slot, `GameplayInteractionSmartObjectBehaviorDefinition` default behavior
- [x] Created `/Game/Foreman/BP_WorkStation` — Actor Blueprint with SmartObjectComponent (definition: SOD_Build), StaticMesh (placeholder cube), implements `IWytchWorkSite` interface
- [x] Placed `WorkStation_Build_01` in NPCLevel near Foreman
- [x] PIE verified: SmartObject subsystem registers the slot, claim/use/release cycle confirmed via LogSmartObject verbose

**Phase 4 completion: ST_Foreman created and validated ✓ (2026-02-28)**
- [x] Created `/Game/Foreman/ST_Foreman` — StateTree with StateTreeAIComponentSchema
  - States: Root (group) → Idle, Dispatch (group) → Plan + Assign, Monitor
  - Tasks: FForemanTask_Wait (Idle), FForemanTask_PlanJob (Plan), FForemanTask_AssignWorker (Assign), FForemanTask_Monitor (Monitor)
  - Enter conditions on Dispatch: FForemanCondition_HasAvailableWork + FForemanCondition_HasIdleWorkers
  - Global evaluator: FForemanEval_WorkAvailability (scans every 0.5s)
  - Transitions: Idle→Root, Plan→Assign, Assign→Monitor, Monitor→Root, Plan→Root (fail), Assign→Root (fail)
- [x] Wired into AIC_Foreman via `ConstructorHelpers::FObjectFinder` in constructor
- [x] Fixed BeginPlay timing: `SetStateTree()` + `StartLogic()` retry after `Super::BeginPlay()` (OnPossess fires before SetStateTree)
- [x] PIE verified: Foreman enters Idle, loops Wait 5s → re-evaluate Root → Dispatch conditions fail (no tagged workers) → Idle

**Roadmap Step 2: Foreman SmartObject Queries (do not start until Step 1 is validated)** ← READY
- [ ] Implement worker roster (Option A: RegisterWorker/UnregisterWorker on AIC_Foreman)
- [ ] Add `GetOwnCondition()` to `AIC_Foreman` for self-monitoring
- [ ] Refactor `FForemanTask_PlanJob` to query SmartObject subsystem
- [ ] Refactor `FForemanCondition_HasAvailableWork` to query SmartObject subsystem
- [ ] Refactor `FForemanCondition_HasIdleWorkers` to query worker roster via `IWytchCommandable::GetWorkerState()`
- [ ] Refactor `FForemanEval_WorkAvailability` to SmartObject queries on timer
- [ ] Refactor `FForemanTask_AssignWorker` to send `FSmartObjectClaimHandle` + `FSmartObjectSlotHandle` + TargetActor via `IWytchCommandable`
