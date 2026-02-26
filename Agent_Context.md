# TheWytching: Incoming Agent Context

## 1. Project Overview

`TheWytching` is an Unreal Engine prototype focused on AI-directed NPC coordination in a sandbox scene.  
Current playable/editor test map is:

- `/Game/Levels/NPCLevel.NPCLevel`

Core concept in active development:

- A Foreman dispatch system that finds idle workers, finds available work targets, claims work atomically, and sends worker commands (`Build`).
- A separate cognitive/vision thread (Foreman + LLM + scene capture) exists in C++ and UI assets, but the old on-screen command widget path has been intentionally disabled.

Current short-term goal:

- **C++-first StateTree architecture for the Foreman.** The Blueprint dispatch loop (Phase 1b/2a) is validated and will remain functional as fallback, but all new Foreman decision-making moves to a `UStateTreeAIComponent` driven by C++ tasks, conditions, and evaluators. BT_Foreman is archived (not deleted).

## 2. Tech Stack

### Engine / Platform

- Engine association: `5.7` (from `TheWytching.uproject`)
- Desktop-focused, DX12 enabled in config.

### Language Split

- **Structural AI, controllers, and data types: C++.** StateTree tasks, conditions, evaluators, AIController, BrainComponent, perception — all in C++.
- **Animations, designer-tweakable variables, visual debugging: Blueprint.**
- C++ module: `Source/TheWytching` for Foreman AI stack, Ollama debug, cognitive JSON utilities.

### Core Modules / Dependencies (C++)

From `Source/TheWytching/TheWytching.Build.cs`:

- `HTTP`, `Json`, `JsonUtilities`, `ImageWrapper`
- `AIModule`, `NavigationSystem`
- `GameplayAbilities`, `GameplayTags`, `GameplayTasks`
- `StateTreeModule`, `GameplayStateTreeModule`

### Key Plugins Enabled

From `TheWytching.uproject`:

- `Mover`, `MoverIntegrations`, `NetworkPrediction`
- `SmartObjects`, `GameplayInteractions`, `GameplayBehaviors`
- `Chooser`, `PoseSearch`, `AnimationWarping`, `MotionWarping`, `AnimationLocomotionLibrary`, `Locomotor`
- `AgentIntegrationKit`
- `MetaHuman`, `MetaHumanCoreTech`, `MetaHumanCharacter`
- `StateTree`
- `ModelingToolsEditorMode` (Editor)

## 3. Developer Profile (Operational)

Observed working style and preferences:

- **Prefers C++ for all structural/AI code.** Blueprint only for animations, designer-tweakable vars, visual debug.
- Works rapidly with direct in-editor validation.
- Prefers pragmatic sequencing and end-to-end validation over premature optimization.
- Requests deterministic debugging: explicit `Print String` traces and concrete runtime evidence.
- Sensitive to crashes/regressions; wants risky UI/cognitive display hooks disabled when unstable.
- Expects incoming agent to continue immediately without re-discovery.

Retention considerations for incoming LLM:

- Preserve current validated Blueprint loop behavior as fallback during StateTree migration.
- Avoid adding duplicate interface event/function graphs (known `ReceiveCommand` duplicate graph issue).
- C++ compiles require editor restart when adding new modules/plugins (Live Coding can't handle new deps).
- Keep runtime logs readable — use `LogForeman` category (defined in `ForemanTypes.h`) for all Foreman C++ logs.
- The `Foreman_BrainComponent` now lives on `AForeman_AIController`, not on `AForeman_Character`.

## 4. Current Architecture

### A. Worker Command Loop (Blueprint)

Main Foreman orchestrator:

- Asset: `/Game/Blueprints/BP_FOREMAN_V2`
- Key variables:
  - `RegisteredWorkers` (`Array<Actor>`)
  - `RegisteredWorkStations` (`Array<Actor>`)
  - `WorkDispatchInterval` (`real`, currently used at `1.5s`)
- Key functions:
  - `RegisterWorker(Worker)`
  - `RegisterWorkStation(WorkStation)`
  - `RefreshRegisteredWorkers()`
  - `FindIdleWorker()`
  - `FindAvailableWork()`
  - `AssignWork(Worker, WorkStation)`
  - `RunWorkAssignmentLoop()`

Current loop behavior:

1. `RunWorkAssignmentLoop` fires on timer.
2. `RefreshRegisteredWorkers` rebuilds worker list from `BPI_WorkerCommand`.
3. `FindIdleWorker` scans workers and checks `GetWorkerState() == Idle`.
4. `FindAvailableWork` scans registered workstations and checks `IsTaskAvailable()`.
5. `AssignWork` calls `TryClaimTask`.
6. On claim success, Foreman sends `Build` command to worker.

### B. Work Target System (Blueprint)

Workstation asset:

- `/Game/Blueprints/Work/BP_WorkStation`
- Variables:
  - `TaskType` (`E_WorkTaskType`)
  - `bIsAvailable` (`bool`)
  - `ClaimedBy` (`Actor`)
- Functions:
  - `RegisterWithForeman()` (BeginPlay self-registration)
  - `IsTaskAvailable()`
  - `TryClaimTask(ClaimingWorker)`
  - `GetTaskType()`
  - `ReleaseTaskLocal(ReleasingWorker)`

Claim atomicity currently enforced:

- `TryClaimTask` branches on `bIsAvailable`, then immediately sets:
  - `bIsAvailable=false`
  - `ClaimedBy=ClaimingWorker`

### C. Worker Contract + State

Worker interface:

- `/Game/Blueprints/BPI_WorkerCommand`
- Functions:
  - `ReceiveCommand(Command : Name)`
  - `GetWorkerState()`
  - `GetCurrentTask()`

Worker state enum:

- `/Game/Foreman/E_WorkerState`
- Values:
  - `NewEnumerator0 = Idle`
  - `NewEnumerator1 = Moving To Task`
  - `NewEnumerator2 = Working`
  - `NewEnumerator3 = Returning`
  - `NewEnumerator4 = Unavailable`

Primary worker implementation used in validated loop:

- `/Game/Blueprints/SandboxCharacter_Mover`
- Interface implementation graph:
  - `ReceiveCommand_Impl`
    - Sets `CurrentCommand`
    - Sets `WorkerState = NewEnumerator1` (Moving To Task)
    - Logs `"Worker received command"`

Secondary worker class:

- `/Game/Bots/BP_AutoBot`
- Implements `BPI_WorkerCommand` (`GetWorkerState`, `GetCurrentTask`)

### D. Foreman Search Command Path (Player Controller)

- `/Game/Blueprints/PC_Sandbox`
- Function: `CommandForemanSearch`
- Contains `GetAIController` node (`F509B610...`) fed by Foreman `Array Element`.
- Safety/debug now present before this path:
  - `Is Valid` (`AE5E6F03...`) on the array element reference.
  - Display name print of the same reference (`Get Display Name` + `Print String`).

### E. Player Character

- Player spawns as `OllamaDronePawn` (C++ class in `Source/TheWytching`).
- Player controller: `/Game/Blueprints/PC_Sandbox` (has `CommandForemanSearch` for manual Foreman commands).
- Game mode: `/Game/Blueprints/GM_Sandbox` (`DefaultPawnClass=OllamaDronePawn`, `PlayerControllerClass=PC_Sandbox`).
- The player is an observer/commander — not the Foreman. BP_FOREMAN_V2 is an autonomous AI actor on the level.

### F. Foreman C++ Architecture

All structural Foreman AI is in C++ under `Source/TheWytching/`.

#### F1. AForeman_Character (`Foreman_Character.h/.cpp`)

- Inherits `ACharacter` + `IAbilitySystemInterface`.
- Sets `AIControllerClass = AForeman_AIController::StaticClass()`.
- `AutoPossessAI = PlacedInWorld`.
- Has `UAbilitySystemComponent` (GAS).
- Capsule, mesh (SK_Mannequin), and movement configured in constructor.
- **No longer owns ForemanBrain** — that moved to the AIController.
- **Not used on the level** — `BP_FOREMAN_V2` (Blueprint Character) is the active Foreman. This class exists for future C++ Foreman characters.

#### F2. AForeman_AIController (`Foreman_AIController.h/.cpp`)

- Inherits `AAIController`.
- **Owns three subobjects:**
  - `UStateTreeAIComponent* StateTreeAI` — drives `ST_Foreman` (StateTree asset, pending creation).
  - `UAIPerceptionComponent` — sight configured in C++ (`ConfigurePerception()`): radius 3000, lose 3500, peripheral 90°, detect all affiliations.
  - `UForeman_BrainComponent* ForemanBrain` — LLM/vision cognitive pipeline (moved here from Character).
- `OnPossess()`: ensures walking mode, calls `ConfigurePerception()`, logs StateTree status.
- `OnUnPossess()`: cleanup logging.
- `ExecuteMoveToLocation()` / `ExecuteMoveToActor()`: pathfinding helpers with fallback to direct movement.
- `GetBrainComponent()`: BlueprintCallable accessor for StateTree tasks to reach the LLM pipeline.

#### F3. UForeman_BrainComponent (`Foreman_BrainComponent.h/.cpp`)

- LLM/vision cognitive engine. **Now owned by AIController, not Character.**
- Scene capture (`USceneCaptureComponent2D`) attached to head socket.
- Perception: finds controller's `UAIPerceptionComponent` or creates its own fallback.
- HTTP calls to local LLM endpoint (`http://localhost:1234/v1/chat/completions`).
- State machine: `EForemanState` (Idle, LookingAround, NavigatingToTarget, PickingUp, NavigatingToDestination, Placing, TaskComplete).
- `IssueCommand(FString)`: entry point for external commands (callable from StateTree tasks).
- `GetForemanController()`: already handles being owned by either Character or Controller.

#### F4. StateTree Tasks (`ForemanStateTreeTasks.h/.cpp`)

Custom `FStateTreeTaskCommonBase` structs for each Foreman phase:

| Task | DisplayName | Behavior |
|---|---|---|
| `FForemanTask_PlanJob` | "Foreman: Plan Job" | Scans world for available workstations + idle workers. Scores by distance. Returns best target/worker pair. Completes instantly on enter. |
| `FForemanTask_AssignWorker` | "Foreman: Assign Worker" | Claims workstation, sends Build command to paired worker. Currently delegates to Blueprint dispatch. |
| `FForemanTask_Monitor` | "Foreman: Monitor" | Oversight mode. Ticks at `CheckInterval` (default 3s). Succeeds when no active assignments remain. |
| `FForemanTask_Rally` | "Foreman: Rally Workers" | Calls all Worker-tagged actors to Foreman's position. Completes instantly. |
| `FForemanTask_Wait` | "Foreman: Wait" | Idles for `Duration` (default 5s). Optionally triggers BrainComponent LLM scan (`bTriggerLLMScan`). |

All tasks use `FForemanTaskInstanceData` (Controller + Pawn context). PlanJob and Monitor have extended instance data.

**TODO:** Tasks currently use tag-based world queries (`"Worker"`, `"WorkStation"`, `"Claimed"`, `"Idle"`) as a temporary bridge. Must be replaced with C++ interface calls (`BPI_WorkerCommand::GetWorkerState`, `BPI_WorkTarget::IsTaskAvailable`) once those interfaces are migrated to C++.

#### F5. StateTree Conditions (`ForemanStateTreeConditions.h/.cpp`)

| Condition | DisplayName | Returns true when |
|---|---|---|
| `FForemanCondition_HasIdleWorkers` | "Foreman: Has Idle Workers" | ≥1 Worker-tagged actor has "Idle" tag |
| `FForemanCondition_HasAvailableWork` | "Foreman: Has Available Work" | ≥1 WorkStation-tagged actor lacks "Claimed" tag |

Same TODO as tasks re: tag-based → interface-based queries.

#### F6. StateTree Evaluator (`ForemanStateTreeEvaluators.h/.cpp`)

| Evaluator | DisplayName | Behavior |
|---|---|---|
| `FForemanEval_WorkAvailability` | "Foreman: Work Availability" | Ticks at `ScanInterval` (default 0.5s). Outputs: `IdleWorkerCount`, `AvailableWorkCount`, `ActiveAssignmentCount`. |

#### F7. Shared Types (`ForemanTypes.h/.cpp`)

- `DECLARE_LOG_CATEGORY_EXTERN(LogForeman, Log, All)` — unified log category for all Foreman C++ code.

#### F8. Cognitive Map Utility

- `Source/TheWytching/CognitiveMapJsonLibrary.*`
- Reads/writes `Saved/Cognitive/CognitiveMap.json`

#### F9. Debug-only LLM Actor

- `Source/TheWytching/OllamaDebugActor.*`

#### G. C++ Interface Hierarchy (NEW — 2026-02-26, awaiting first compile)

Foundation for all world interactions in TheWytching. Replaces the Blueprint-only `BPI_WorkerCommand`/`BPI_WorkTarget` interfaces.

**Design principle:** Workers receive generic commands ("go to X, interact"). The object itself defines what "interact" means via the interface it implements.

| Interface | Inherits | Purpose | Key Functions |
|---|---|---|---|
| `IWytchInteractable` | — | Base. Anything in the world that can be interacted with. | `GetInteractionType()`, `IsInteractionAvailable()`, `GetInteractionLocation()` |
| `IWytchCarryable` | `IWytchInteractable` | Anything that can be picked up and moved. | `OnPickedUp(Carrier)`, `OnPutDown(DropLocation)`, `GetDeliveryLocation()`, `IsAtDestination()` |
| `IWytchWorkSite` | `IWytchInteractable` | Anywhere work can be performed. | `TryClaim(Worker)`, `ReleaseSite(Worker)`, `GetClaimant()`, `GetWorkType()` |

All functions are `BlueprintNativeEvent` + `BlueprintCallable` — C++ provides defaults via `_Implementation()`, Blueprints can override.

**UInterface inheritance note:** `UWytchCarryable : public UWytchInteractable` and `UWytchWorkSite : public UWytchInteractable` on the U-side; matching I-side inheritance. This is supported in UE5 but can be tricky — if compile fails on inheritance, fallback plan is to flatten to independent interfaces that each redeclare base methods.

**Status:** Files written, awaiting editor restart for full UHT + compile pass. Live Coding cannot handle new `GENERATED_BODY()` files.

## 5. Asset Inventory (Key, Active)

### Core Blueprints

- `/Game/Blueprints/BP_FOREMAN_V2`
  - Autonomous AI Foreman. Dispatch orchestration, worker/workstation lists, assignment loop.
  - Uses `AIC_NPC_SmartObject` (StateTree-based AIController, `AutoPossessAI=PlacedInWorld`).
  - Not player-possessed. Input bindings in EventGraph are inherited from parent `Character` template (dead code for AI usage).
- `/Game/Blueprints/PC_Sandbox`
  - Player controller. Input command entry point (`G`) and `CommandForemanSearch`.
- `/Game/Blueprints/GM_Sandbox`
  - Level game mode. Player=`OllamaDronePawn`, Controller=`PC_Sandbox`.
- `/Game/Blueprints/SandboxCharacter_Mover`
  - Main worker class with `BPI_WorkerCommand` implementation.
- `/Game/Bots/BP_AutoBot`
  - Alternate worker class implementing worker interface.
- `/Game/Blueprints/Work/BP_WorkStation`
  - Work target registration/claim/release.
- `/Game/Blueprints/Work/BPI_WorkTarget`
  - Work target interface contract (exists, but see gotchas).
- `/Game/Blueprints/Work/E_WorkTaskType`
  - Currently only `Build`.
- `/Game/Foreman/E_WorkerState`
  - Worker state source-of-truth enum.

### AI / Behavior Assets

- `/Game/Foreman/BT_Foreman` — **Archived.** Dormant BT with EQS queries + custom tasks. BB=BB_Foreman (manually fixed from BB_AIPAWN). Superseded by StateTree architecture. Retained as reference for reimplementing oversight/patrol as ST tasks.
- `/Game/Foreman/BB_Foreman` — Blackboard for BT_Foreman. 7 keys. Not used by StateTree path.
- `/Game/Foreman/EQ_FindWorkers` — EQS query. May be reused from StateTree tasks in future.
- `/Game/Foreman/EQ_FindWorkTargets` — EQS query. May be reused from StateTree tasks in future.
- `/Game/Foreman/EQ_FindOversightPoint` — EQS query for Monitor state.
- `/Game/Foreman/ST_Foreman` — **Active, validated.** StateTree asset (StateTreeAIComponentSchema). 5 states, 10 tasks, 4 transitions, 4 conditions, 1 evaluator. Wired to `AIC_Foreman`'s inherited `StateTreeAIComponent`. Working as of 2026-02-26.

### C++ Source Files (`Source/TheWytching/`)

| File | Purpose |
|---|---|
| `ForemanTypes.h/.cpp` | `LogForeman` log category, shared types |
| `Foreman_AIController.h/.cpp` | AIController: StateTreeAIComponent + Perception + BrainComponent |
| `Foreman_BrainComponent.h/.cpp` | LLM/vision cognitive pipeline (owned by controller) |
| `Foreman_Character.h/.cpp` | Base character class with GAS (not on level) |
| `ForemanStateTreeTasks.h/.cpp` | 5 StateTree tasks: PlanJob, AssignWorker, Monitor, Rally, Wait |
| `ForemanStateTreeConditions.h/.cpp` | 2 StateTree conditions: HasIdleWorkers, HasAvailableWork |
| `ForemanStateTreeEvaluators.h/.cpp` | 1 StateTree evaluator: WorkAvailability |
| `CognitiveMapJsonLibrary.h/.cpp` | JSON read/write for cognitive map |
| `OllamaDebugActor.h/.cpp` | Debug LLM actor |
| `OllamaDebugSpawner.h/.cpp` | Debug spawner |
| `OllamaDronePawn.h/.cpp` | Player pawn (C++) |
| `PC_Wytching.h/.cpp` | Player controller (C++) |
| `PatrolPoint.h/.cpp` | Patrol waypoint actor |
| `ForemanSurveyComponent.h/.cpp` | Survey/scan component for Foreman. `DiscoverInteractables(SearchRadius)` — pure interface-based discovery via `GetAllActorsWithInterface<UWytchInteractable>`, distance-filtered, no tags. |
| `IWytchInteractable.h/.cpp` | **NEW (2026-02-26, awaiting compile)** Base UInterface — anything interactable in the world |
| `IWytchCarryable.h/.cpp` | **NEW (2026-02-26, awaiting compile)** UInterface extending Interactable — pickable/movable objects |
| `IWytchWorkSite.h/.cpp` | **NEW (2026-02-26, awaiting compile)** UInterface extending Interactable — work locations |

### Widget Assets (currently detached/disabled path)

- `/Game/WBP_CommandInput`
- `/Game/Blueprints/WBP_CognitiveDisplay`
- `/Game/Blueprints/WBP_CognitiveEntry`

## 6. Design Decisions (Why)

### Interface message call vs typed call

- `AssignWork` now supports both:
  - Preferred deterministic path for current worker: cast to `SandboxCharacter_Mover` then call `ReceiveCommand_Impl`.
  - Fallback: `ReceiveCommand (Message)` on generic interface object.
- Reason: message call is robust across interface implementers; typed call was needed to guarantee delivery during active debugging when interface wiring mismatched in practice.

### Worker state as source of truth

- Foreman does not infer state from movement; it queries `GetWorkerState()`.
- Reason: explicit state machine keeps assignment logic simple and testable.

### Self-registration for workstations

- `BP_WorkStation` registers itself with Foreman on `BeginPlay`.
- Reason: scales to dynamically spawned/procedural stations better than central scan-only ownership.

### Timer-based dispatch loop (not Tick)

- `RunWorkAssignmentLoop` is executed by repeating timer (`WorkDispatchInterval`).
- Reason: lower noise, easier debugging, avoids frame-by-frame thrash.

### Atomic claim on station

- Claim and availability flip happen in `TryClaimTask` in same call path.
- Reason: prevents two workers claiming same target on adjacent frames.

### StateTree over Behavior Tree for Foreman

- The Foreman's decision-making uses `UStateTreeAIComponent` + C++ StateTree tasks, not BT.
- Reason: The Foreman is fundamentally state-based (Dispatch → Monitor → Rally → Idle). StateTrees model explicit states with conditional transitions more naturally than BT priority fallback chains. Evaluators provide continuous data (worker/work counts) without timer polling. SmartObjects integration is native. Epic's active investment path.
- `BT_Foreman` is archived — its behaviors (CallAllToMe, EQS queries, IssueCommand) are reimplemented as StateTree tasks.

### BrainComponent on AIController, not Character

- `UForeman_BrainComponent` is a subobject of `AForeman_AIController`, not `AForeman_Character`.
- Reason: The controller persists across repossession. StateTree tasks access the brain via the controller context. BP_FOREMAN_V2 (the active Foreman) is a Blueprint Character that doesn't inherit from `AForeman_Character`, so placing the brain on the controller ensures it's always accessible regardless of which Character class the Foreman uses.

### Tag-based queries as temporary bridge

- C++ StateTree tasks/conditions currently use actor tags (`"Worker"`, `"WorkStation"`, `"Claimed"`, `"Idle"`) to find and query actors.
- Reason: The Blueprint interfaces (`BPI_WorkerCommand`, `BPI_WorkTarget`) are not yet callable from C++. Once these interfaces are migrated to C++ (or a C++ `UInterface` equivalent is created), the tag-based queries will be replaced with proper interface calls.

## 7. Current Build State

### C++ — Compiled and working

StateTree architecture fully compiled and running:
- `ForemanTypes.h/.cpp`, `ForemanStateTreeTasks.h/.cpp`, `ForemanStateTreeConditions.h/.cpp`, `ForemanStateTreeEvaluators.h/.cpp`
- `Foreman_AIController.h/.cpp`, `Foreman_Character.h/.cpp`, `Foreman_BrainComponent.h/.cpp`
- `ForemanSurveyComponent.h/.cpp`, `PC_Wytching.h/.cpp`, `PatrolPoint.h/.cpp`

### C++ — Awaiting editor restart to compile (2026-02-26)

New `IWytch*` interface files (6 files, all `GENERATED_BODY()`):
- `IWytchInteractable.h/.cpp`
- `IWytchCarryable.h/.cpp`
- `IWytchWorkSite.h/.cpp`

Live Coding cannot handle new files — requires full editor restart for UHT pass.

### Blueprint — Validated (compiled + runtime evidence)

Compiled clean (0 errors/warnings):

- `/Game/Blueprints/BP_FOREMAN_V2`
- `/Game/Blueprints/Work/BP_WorkStation`
- `/Game/Blueprints/SandboxCharacter_Mover`
- `/Game/Bots/BP_AutoBot`
- `/Game/Blueprints/PC_Sandbox`
- `/Game/Blueprints/BP_AIPAWN`

Runtime sequence confirmed (Phase 1b/2a loop):

- `WS RegisterWithForeman: BP_WorkStation`
- `Foreman: idle worker + workstation found, calling AssignWork`
- `WS TryClaimTask success: bIsAvailable=false ClaimedBy=SandboxCharacter_Mover2`
- `Foreman AssignWork: TryClaimTask succeeded -> sending Build`
- `Worker received command`

### Blueprint — Temporary dead code in EventGraph

BP_FOREMAN_V2's EventGraph has orphan nodes from earlier BT wiring attempt (added before StateTree decision):
- `Get AIController` node (GUID: `2D4AA8C5`)
- `Run Behavior Tree` node (GUID: `8823C383`, BTAsset=BT_Foreman)
- Sequence `then_2` pin targeting the above

These should be **removed** during Phase 4 Integration cutover.

## 8. Active Roadmap

### Completed Phases (Blueprint Foundation)

1. **Phase 1a - Worker registration/state contract** — Complete.
2. **Phase 1b - Foreman assignment loop** — Complete, runtime-validated.
3. **Phase 2a - WorkStation foundation** — Complete, runtime-validated.

### Current: C++ StateTree Architecture Migration

| Phase | Scope | Status |
|---|---|---|
| **Phase 0** | Foundation: Build.cs, .uproject, AIController refactor, ForemanTypes | **Complete** |
| **Phase 1** | StateTree Tasks: PlanJob, AssignWorker, Monitor, Rally, Wait | **Complete** |
| **Phase 2** | Conditions (HasIdleWorkers, HasAvailableWork) + Evaluator (WorkAvailability) | **Complete** |
| **Phase 3** | Create `ST_Foreman` StateTree asset, build state hierarchy, wire transitions | **Complete — ST_Foreman working (5 states, 10 tasks, 4 transitions, 4 conditions, 1 evaluator)** |
| **Phase 4** | Integration: AIC_Foreman active, StateTree starts on possess | **Complete — validated, bStartLogicAutomatically=True** |
| **Phase 5** | Cleanup: tag-based queries → C++ interfaces | **In progress — IWytch* interfaces written + `DiscoverInteractables` added to ForemanSurveyComponent, all awaiting editor restart to compile** |

### Current: IWytch* Interface Hierarchy + Block Transport Demo

| Phase | Scope | Status |
|---|---|---|
| **Interfaces** | `IWytchInteractable`, `IWytchCarryable`, `IWytchWorkSite` C++ UInterfaces | **Code written, awaiting editor restart to compile** |
| **Gait Fix** | BP_FOREMAN_V2 walks by default, `bForceRun` for running | **Complete — compiled clean** |
| **Task Infra** | Add Transport to E_WorkTaskType, BP_Block, BP_TransportStation | Pending |
| **Worker Commands** | Implement ReceiveCommand on BP_AutoBot (navigate, pickup, deliver) | Pending |
| **Level Setup** | Blocks at A, transport stations, dropoff at B | Pending |
| **Wire & Test** | Foreman dispatches all workers to carry blocks A→B | Pending |

### Future

- ~~Migrate `BPI_WorkerCommand` / `BPI_WorkTarget` to C++ `UInterface`~~ — **In progress.** New `IWytch*` interface hierarchy replaces this (see Section 4G).
- `FForemanTask_SourceMaterials` — when a material/resource system exists.
- Reintroduce redesigned cognitive display widget.
- Replace temporary debug prints with structured `LogForeman` category exclusively.

## 9. Open Issues / Known Gotchas

- **Duplicate graph error gotcha:**  
  Error seen: `"Graph named 'ReceiveCommand' already exists..."`  
  Do not attempt to re-add/auto-generate interface function graphs if they already exist.

- **`BP_WorkStation` interface mismatch risk:**  
  `BPI_WorkTarget` exists, but `BP_WorkStation` currently reports `interfaces=0` (no formal implementation) while Foreman uses cast-to-`BP_WorkStation` for calls.  
  This works now, but is design debt. Will be resolved when interfaces are migrated to C++.

- **PIE compile constraint:**  
  Blueprint compile fails during PIE. Stop PIE before compile.

- **Widget visibility:**  
  Old command/cognitive widget flow is intentionally detached. Keep disabled until replacement.

- **New module deps require editor restart:**  
  `StateTreeModule` and `GameplayStateTreeModule` were added to Build.cs. `StateTree` plugin added to .uproject. Live Coding cannot handle new module dependencies — **full editor restart required** for first compile.

- **Tag-based query bridge (temporary):**  
  C++ StateTree tasks/conditions use actor tags (`"Worker"`, `"WorkStation"`, `"Claimed"`, `"Idle"`) instead of interface calls. This is a deliberate temporary bridge. Workers and workstations must have these tags for the C++ path to find them. The Blueprint dispatch loop uses proper interface calls and continues to work independently.

- **Dead BT nodes in BP_FOREMAN_V2 EventGraph:**  
  `Get AIController` (2D4AA8C5) and `Run Behavior Tree` (8823C383) were added before the StateTree decision. They're connected to Sequence `then_2` but do nothing (wrong controller type). Remove during Phase 4.

- **BT_Foreman is archived, not deleted:**  
  Contains custom C++ BTTasks (`BTTask_CallAllToMe`, `BTTask_IssueCommand`) and EQS query references that may be useful as reference when implementing equivalent StateTree tasks. Do not delete. BlackboardAsset was manually fixed to BB_Foreman (was incorrectly BB_AIPAWN).

- **AIC_Foreman StateTree startup fix (2026-02-25):**  
  Three compounding issues prevented `ST_Foreman` from ever starting:  
  1. **Wrong StateTree on component** — inherited `StateTreeAIComponent` referenced `ST_NPC_SandboxCharacter_SmartObject` instead of `ST_Foreman`. Fixed: component now points to `/Game/Foreman/ST_Foreman`.  
  2. **Orphaned BeginPlay blocked parent startup** — AIC_Foreman had a disconnected `Event BeginPlay` node that overrode (and silently blocked) the parent class `AIC_NPC_SmartObject`'s BeginPlay, which contains the `Start Logic` call. Fixed: deleted the orphaned node so parent's BeginPlay executes.  
  3. **Broken RunStateTree call** — `OnPossess` called `RunStateTree(PossessedPawn, ST_Foreman)`, but the pawn has no `StateTreeComponent` (it's on the controller). This silently failed every time. Fixed: removed `RunStateTree`, "State Tree Started" print, and floating `Get Component by Class`/`Get StateTreeAI` nodes. Kept `OnPossess → Print "OnPossess FIRED"` for debug.  
  **Lesson:** In Blueprint inheritance, a child class with an `Event BeginPlay` node (even if disconnected) **overrides the parent's BeginPlay entirely**. The parent's logic only runs if the child explicitly calls `Parent: Event BeginPlay`. Always verify parent event flow isn't silently blocked.  
  **Note:** `bStartLogicAutomatically` is now `True` on the component (changed from False during later testing/tweaking). StateTree starts automatically on possess.

- **AIC_Foreman is the active AI Controller for BP_FOREMAN_V2:**  
  `AIControllerClass = AIC_Foreman` (inherits from `AIC_NPC_SmartObject`). StateTree startup is handled by the parent class's BeginPlay → 2s delay → `Start Logic` on the `StateTreeAIComponent`. The C++ `AForeman_AIController` may replace this in a future phase.

- **Foreman_BrainComponent perception fallback:**  
  The BrainComponent's `InitialiseComponents()` auto-detects whether to use the controller's perception or create its own. Since the controller now creates perception in C++, the BrainComponent should find and reuse it. Verify after first runtime test.

- **BP_FOREMAN_V2 Gait Override (2026-02-26):**  
  The Foreman's parent Character class has a player-input-driven `GetDesiredGait()` function called every frame by `UpdateMovement_PreCMC`. For the AI-controlled Foreman (no player input), this produced incorrect gait selection. **Fix:** Added `bForceRun` bool variable (default false, category "AI Movement"). Short-circuited `GetDesiredGait` entry → Branch on `bForceRun` → false=Walk, true=Run. Old player-input logic is disconnected but preserved in the graph. The Foreman now walks by default. StateTree tasks can set `bForceRun = true` when urgency is needed (e.g., Rally).

- **IWytch* interface files require editor restart to compile (2026-02-26):**  
  6 new C++ files with `GENERATED_BODY()` added: `IWytchInteractable`, `IWytchCarryable`, `IWytchWorkSite` (.h + .cpp each). Live Coding cannot handle new files — requires full editor restart for UHT to generate reflection code. If UInterface inheritance (`UWytchCarryable : public UWytchInteractable`) causes compile errors, flatten to independent interfaces.

- **Worker interface status discrepancy:**  
  `BP_AutoBot` (4 in level) implements `BPI_WorkerCommand` (GetWorkerState, GetCurrentTask) but has **no ReceiveCommand handler** — just auto-wanders. `SandboxCharacter_Mover` (2 in level) is tagged "Worker" but does **NOT implement BPI_WorkerCommand at all**. The 3x `SandboxCharacter_CMC` are tagged "Legacy" and are not workers. Only AutoBots are viable workers currently.

## 10. Immediate Next Step (Pick Up Here)

### Step 1: Restart Editor — Compile IWytch* Interfaces + DiscoverInteractables

Close and reopen Unreal Editor. The engine will run UHT on the 6 new interface files and compile `ForemanSurveyComponent` changes together. Watch for:

- **UInterface inheritance errors** — `UWytchCarryable : public UWytchInteractable` may not be supported. If so, flatten to independent interfaces.
- **Missing includes** — verify `UObject/Interface.h` resolves correctly.
- **THEWYTCHING_API linkage** — ensure the API macro exports correctly for interface classes.

### Step 2: Implement Foreman's Site Discovery Loop

`ForemanSurveyComponent::DiscoverInteractables(float SearchRadius)` is written and ready to compile. After successful compile:

1. Wire `DiscoverInteractables` into StateTree tasks (replace tag-based queries in `FForemanTask_PlanJob`, `FForemanCondition_HasAvailableWork`, etc.).
2. PIE test: verify Foreman discovers actors implementing `IWytchInteractable` / `IWytchCarryable` / `IWytchWorkSite` by interface, not tags.
3. Gradually remove tag-based fallback queries from C++ StateTree code.

### Architecture Vision

The `IWytch*` interface hierarchy becomes the foundation for everything:
- **IWytchInteractable** — base for all world objects
- **IWytchCarryable** — blocks, resources, items (anything movable)
- **IWytchWorkSite** — build sites, harvest points, crafting stations

`DiscoverInteractables` is the first concrete consumer of this hierarchy — pure interface discovery, no tags, no casts. Workers receive generic commands ("go to X, interact"). The object's interface defines the behavior. Foreman queries `IWytchInteractable` to find work, `IWytchWorkSite::TryClaim()` for atomic assignment, `IWytchCarryable` for transport tasks. This replaces the tag-based query bridge and the Blueprint-only `BPI_WorkerCommand`/`BPI_WorkTarget` interfaces.
