# foreman_ai — Module Context
`~600 tokens  TheWytching  Aisling Brain System`

---

## What This Module Is

The Foreman AI system. `BP_FOREMAN_V2` is the level actor (thin Blueprint shell). `AForeman_AIController` drives all logic via StateTree + BrainComponent.

**Active files:**
- `Foreman_AIController.h/.cpp` — AIController; owns StateTreeAI, ForemanBrain, Perception
- `ForemanStateTreeTasks.h/.cpp` — PlanJob, AssignWorker, Monitor, Rally, Wait
- `ForemanStateTreeConditions.h/.cpp` — HasIdleWorkers, HasAvailableWork
- `ForemanStateTreeEvaluators.h/.cpp` — WorkAvailability (0.5s timer scan)
- `ForemanTypes.h/.cpp` — LogForeman log category
- `Foreman_BrainComponent.h/.cpp` — LLM/vision pipeline (stable, don't touch)
- `ForemanSurveyComponent.h/.cpp` — secondary discovery for non-SmartObject items (stable, reduced scope)

**Content:** `/Game/Foreman/ST_Foreman` — StateTree asset. States: Root→Idle→Dispatch(Plan+Assign)→Monitor.

---

## Current State (pre-Step-2)

All StateTree tasks, conditions, and evaluator use `GetAllActorsWithTag` stubs. These are **intentional transitional placeholders**, not final code. All have `// TODO` comments pointing to the SmartObject migration.

`AIC_Foreman` has **no worker roster** yet (`RegisteredWorkers` array does not exist). `GetOwnCondition()` does not exist.

---

## Step 2 Target Changes (all existing files — Live Coding OK)

| Sub-task | File | Change |
|---|---|---|
| 2a | `Foreman_AIController.h/.cpp` | Add `RegisterWorker`/`UnregisterWorker` + `TArray<TWeakObjectPtr<AActor>> RegisteredWorkers` |
| 2b | `Foreman_AIController.h/.cpp` | Add `GetOwnCondition()` → `UAndroidConditionComponent*` from possessed pawn |
| 2c | `ForemanStateTreeEvaluators.cpp` | `ScanWorld()`: roster+`IWytchCommandable::GetWorkerState()` for workers; `FindSmartObjects()` for work |
| 2d | `ForemanStateTreeConditions.cpp` | `HasIdleWorkers`: use roster query |
| 2e | `ForemanStateTreeConditions.cpp` | `HasAvailableWork`: use SmartObject subsystem |
| 2f | `ForemanStateTreeTasks.cpp` | `PlanJob`: SmartObject query + capability tag matching |
| 2g | `ForemanStateTreeTasks.cpp` | `AssignWorker`: send ClaimHandle + SlotHandle + TargetActor via `IWytchCommandable` |

---

## Key API (Step 2)

```cpp
// Subsystem access
USmartObjectSubsystem* SOSub = USmartObjectSubsystem::GetCurrent(World);

// Query filter — ActivityRequirements is FGameplayTagQuery, NOT FGameplayTagContainer
FGameplayTagContainer TaskTags;
TaskTags.AddTag(FGameplayTag::RequestGameplayTag("Task.Build"));
FSmartObjectRequestFilter Filter;
Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchAnyTags(TaskTags);

FSmartObjectRequest Request(FBox(Origin, FVector(SearchRadius)), Filter);
TArray<FSmartObjectRequestResult> Results;
SOSub->FindSmartObjects(Request, Results);

// Claiming (worker does this, not Foreman)
FSmartObjectClaimHandle ClaimHandle = SOSub->MarkSlotAsClaimed(Result.SlotHandle, ...);

// Worker interface cast
if (IWytchCommandable* Commandable = Cast<IWytchCommandable>(WorkerActor))
{
    Commandable->Execute_ReceiveSmartObjectAssignment(WorkerActor, ClaimHandle, SlotHandle, TargetActor);
}
```

---

## Log Category

`LogForeman` — declared in `ForemanTypes.h`

---

## Gotchas

- `FSmartObjectRequestFilter::ActivityRequirements` is `FGameplayTagQuery` not `FGameplayTagContainer` (DE-003)
- `MarkSlotAsClaimed()` / `MarkSlotAsFree()` — NOT deprecated `Claim()` / `Release()` (DE-002)
- StateTree startup: SetStateTree() + StartLogic() in BeginPlay + retry in OnPossess (PAT-001)
- Foreman mediates assignment — workers do NOT autonomously find SmartObjects (DEC-003)
- Worker holds the claim, not the Foreman (DEC-004)

