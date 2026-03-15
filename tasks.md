# TheWytching — Tasks
**Last updated:** 2026-03-08

---

## 🎯 Active: Roadmap Step 2 — Foreman SmartObject Queries

All changes are to *existing* files → Live Coding OK (Ctrl+Alt+F11). No new modules or GENERATED_BODY files.

- [ ] **2a.** `Foreman_AIController.h/.cpp` — Add `RegisterWorker` / `UnregisterWorker` + `TArray<TWeakObjectPtr<AActor>> RegisteredWorkers`
- [ ] **2b.** `Foreman_AIController.h/.cpp` — Add `GetOwnCondition()` returning `UAndroidConditionComponent*` from possessed pawn
- [ ] **2c.** `ForemanStateTreeEvaluators.cpp` — Refactor `ScanWorld()`: workers via roster+`IWytchCommandable::GetWorkerState()`; work via `USmartObjectSubsystem::FindSmartObjects()`
- [ ] **2d.** `ForemanStateTreeConditions.cpp` — Refactor `HasIdleWorkers`: use roster query instead of `GetAllActorsWithTag`
- [ ] **2e.** `ForemanStateTreeConditions.cpp` — Refactor `HasAvailableWork`: use SmartObject subsystem instead of `GetAllActorsWithTag`
- [ ] **2f.** `ForemanStateTreeTasks.cpp` — Refactor `PlanJob`: SmartObject subsystem query for `Task.Build` slots + capability tag matching
- [ ] **2g.** `ForemanStateTreeTasks.cpp` — Refactor `AssignWorker`: send `FSmartObjectClaimHandle` + `FSmartObjectSlotHandle` + `TargetActor` via `IWytchCommandable::ReceiveSmartObjectAssignment()`

**Key API reminders for Step 2:**
- `USmartObjectSubsystem::GetCurrent(World)`
- `FindSmartObjects(FSmartObjectRequest, TArray<FSmartObjectRequestResult>&)`
- `FSmartObjectRequestFilter::ActivityRequirements` = `FGameplayTagQuery` (use `MakeQuery_MatchAnyTags()`)
- `MarkSlotAsClaimed()` / `MarkSlotAsFree()` — NOT the deprecated `Claim()` / `Release()`
- Worker cast: `Cast<IWytchCommandable>(WorkerActor)`

---

## ✅ Completed

### Roadmap Step 1 — SmartObject Foundation (2026-02-28)
- [x] `SOD_Build` SmartObjectDefinition created (`/Game/Foreman/SmartObjects/SOD_Build`, `Task.Build` tag, 1 slot)
- [x] `BP_WorkStation` Blueprint Actor created (`/Game/Foreman/BP_WorkStation`, SmartObjectComponent + `IWytchWorkSite`)
- [x] `WorkStation_Build_01` placed in NPCLevel
- [x] PIE verified: slot registers with subsystem, claim/use/release confirmed via LogSmartObject verbose

### C++ Foundation — Phases 0–4 (2026-02-28)
- [x] `ForemanTypes.h/.cpp` — `LogForeman`
- [x] `AndroidTypes.h/.cpp` — all enums + `LogWytchAndroid` + `LogWytchWorker`
- [x] `AndroidConditionComponent.h/.cpp` — power, subsystems, degradation
- [x] `IWytchInteractable`, `IWytchCarryable`, `IWytchWorkSite`, `IWytchCommandable` — all compiled ✓
- [x] `AIC_Foreman` — StateTreeAI + BrainComponent + Perception; FObjectFinder wires ST_Foreman; BeginPlay/OnPossess startup retry
- [x] `ST_Foreman` created: Root→Idle→Dispatch(Plan+Assign)→Monitor; evaluator; PIE validated
- [x] `DefaultGameplayTags.ini` — 31 tags (Capability.*, Task.*, Status.*, Mode.*)
- [x] `Build.cs` — SmartObjectsModule added

---

## 📋 Backlog (Roadmap Steps 3–9)

- [ ] **Step 3** — Worker SmartObject interaction (worker receives claim handle, claims on arrival, releases on complete/fail)
- [ ] **Step 4** — Capability tag system: tag General Workers + AutoBots; Foreman filters by `Capability.*` in SmartObject queries
- [ ] **Step 5** — Unified General Worker class (C++): state machine, `IWytchCommandable`, `UAndroidConditionComponent`, SmartObject interaction
- [ ] **Step 6** — Refactor AutoBot: capability tags, SmartObject interaction, `ReceiveCommand` handler
- [ ] **Step 7** — Transport demo: `Task.Transport` SOD, `BP_Block` (`IWytchCarryable`), pickup/dropoff SmartObjects
- [ ] **Step 8** — Quarry demo: AutoBot laser-cuts blocks → General Workers haul
- [ ] **Step 9** — End-to-end test: both worker types, full Foreman orchestration loop

## Future (post-Step 9)
- `FForemanTask_SourceMaterials`
- Soldier Bot class (`Capability.Combat`, `Capability.Patrol`)
- `IWytchDefendPoint`
- Redesigned cognitive display widget
- Replace all debug `Print String` / `GEngine->AddOnScreenDebugMessage` with structured `LogForeman`
