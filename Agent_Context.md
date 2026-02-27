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
- `/Game/Foreman/ST_Foreman` — **Active.** StateTree asset (StateTreeAIComponentSchema). 4 states, 3 tasks, 3 transitions, 2 conditions, 1 evaluator. Wired to `AIC_Foreman`'s inherited `StateTreeAIComponent`.

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

### C++ (awaiting editor restart to compile)

New/modified C++ files written but not yet compiled (requires editor restart for new `StateTreeModule`/`GameplayStateTreeModule` deps):

- `ForemanTypes.h/.cpp` — new
- `ForemanStateTreeTasks.h/.cpp` — new
- `ForemanStateTreeConditions.h/.cpp` — new
- `ForemanStateTreeEvaluators.h/.cpp` — new
- `Foreman_AIController.h/.cpp` — refactored (BT→StateTree)
- `Foreman_Character.h/.cpp` — updated (removed BrainComponent)
- `TheWytching.Build.cs` — added StateTree modules
- `TheWytching.uproject` — added StateTree plugin

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
| **Phase 0** | Foundation: Build.cs, .uproject, AIController refactor, ForemanTypes | **Code written, awaiting compile** |
| **Phase 1** | StateTree Tasks: PlanJob, AssignWorker, Monitor, Rally, Wait | **Code written, awaiting compile** |
| **Phase 2** | Conditions (HasIdleWorkers, HasAvailableWork) + Evaluator (WorkAvailability) | **Code written, awaiting compile** |
| **Phase 3** | Create `ST_Foreman` StateTree asset, build state hierarchy, wire transitions | **Next after compile** |
| **Phase 4** | Integration: swap BP_FOREMAN_V2 AIControllerClass → AForeman_AIController, remove dead BT nodes, disable Blueprint timer loop | Pending |
| **Phase 5** | Cleanup: archive AIC_NPC_SmartObject, update docs, structured logging | Pending |

### Future

- Migrate `BPI_WorkerCommand` / `BPI_WorkTarget` to C++ `UInterface` — eliminates tag-based query bridge.
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
  **Note:** `bStartLogicAutomatically` remains `False` on the component — startup relies on the parent's explicit `Start Logic` call (fires after 2s delay). This is intentional design from `AIC_NPC_SmartObject`.

- **AIC_Foreman is the active AI Controller for BP_FOREMAN_V2:**  
  `AIControllerClass = AIC_Foreman` (inherits from `AIC_NPC_SmartObject`). StateTree startup is handled by the parent class's BeginPlay → 2s delay → `Start Logic` on the `StateTreeAIComponent`. The C++ `AForeman_AIController` may replace this in a future phase.

- **Foreman_BrainComponent perception fallback:**  
  The BrainComponent's `InitialiseComponents()` auto-detects whether to use the controller's perception or create its own. Since the controller now creates perception in C++, the BrainComponent should find and reuse it. Verify after first runtime test.

## 10. Immediate Next Step (Pick Up Here)

### Step 1: Restart Editor and Compile C++

Close and reopen Unreal Editor. The engine will rebuild all C++ because new modules (`StateTreeModule`, `GameplayStateTreeModule`) and a new plugin (`StateTree`) were added. Fix any compile errors:

Likely issues:
- Include path: `#include "Components/StateTreeAIComponent.h"` — verify against UE 5.7 source.
- `UStateTreeAIComponent::GetStateTree()` accessor — may be named differently.
- Module names `"StateTreeModule"` / `"GameplayStateTreeModule"` — standard but unverified.

### Step 2: Create ST_Foreman StateTree Asset (Phase 3)

1. Create StateTree asset `/Game/Foreman/ST_Foreman` with `StateTreeAIComponentSchema`.
2. Build state hierarchy under Root (TrySelectChildrenInOrder):
   - **Dispatch** — Enter conditions: `HasIdleWorkers` + `HasAvailableWork`. Tasks: PlanJob → AssignWorker. Transition: OnStateCompleted → Monitor.
   - **Monitor** — Enter condition: active assignments exist. Task: Monitor. Transition: OnTick if HasIdleWorkers AND HasAvailableWork → Dispatch.
   - **Rally** — Reserved for external command trigger.
   - **Idle** — Fallback. Task: Wait.
3. Add evaluator: `WorkAvailability` at root level.
4. Set `ST_Foreman` as the StateTree on `AForeman_AIController`'s `StateTreeAIComponent`.

### Step 3: Integration + Cutover (Phase 4)

1. Change `BP_FOREMAN_V2`'s `AIControllerClass` property from `AIC_NPC_SmartObject` → `AForeman_AIController`.
2. Remove dead EventGraph nodes: `Get AIController` (2D4AA8C5), `Run Behavior Tree` (8823C383), Sequence `then_2`.
3. Disable Blueprint dispatch timer (`RunWorkAssignmentLoop`) — StateTree now drives dispatch.
4. PIE test: verify Foreman is possessed by `AForeman_AIController`, StateTree starts, states transition, worker gets assigned.

### Step 4: Cleanup (Phase 5)

1. Archive `AIC_NPC_SmartObject` (no longer used by anything).
2. Replace tag-based queries with C++ interface calls (requires `BPI_WorkerCommand`/`BPI_WorkTarget` migration to C++).
3. Finalize `agent_context.md`.
