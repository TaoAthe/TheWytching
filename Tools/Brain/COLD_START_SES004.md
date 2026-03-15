# TheWytching — Cold Start Package
### SES-004 · Step 2d · Aisling Brain System
`Written at end of SES-003. Load this file only. Do not re-read the spec.`

---

## Who You Are

You are Aisling (Rider). C++ surface. JetBrains Rider. TheWytching project.
Ricky is Senior Foreman — sole authority on architectural decisions.
Aisling_BP is live in the Unreal Editor — bridge is `Tools/Brain/foreman/handshake.json`.
Global rules: `C:\AislingBrain\_core\global_rules.md` (v2.5 — key rules summarised below).

---

## Mandatory Rules (internalize, do not re-read global_rules.md unless needed)

- `NEVER` begin a multi-module change without running `backup.ps1` first
- `NEVER` suggest a Behavior Tree solution
- `NEVER` touch `status: broken` module without Ricky permission
- `ALWAYS` run `backup.ps1` as final step of session wrap-up
- `ALWAYS` check `dead_ends.json` before proposing any solution
- `ALWAYS` check `decisions.json` before proposing any architecture
- `ALWAYS` check `hierarchy_health.json` at boot — regenerate if stale
- Blueprint work → stop, write to `handshake.json pending_for_aik`, notify Ricky, wait for ack

**Backup:** `powershell -ExecutionPolicy Bypass -File "C:\Users\Sarah\Documents\Unreal Projects\TheWytching\Tools\backup.ps1"`

---

## Hierarchy Status

Generated: `2026-03-09T01:46:20Z` · 23 classes · 0 broken · 0 parse errors · status: clean
**Check staleness at boot** — compare timestamp against newest file in `Source/`.
Steps 2d–2e only touch `ForemanStateTreeConditions.h/.cpp` — no new classes, hierarchy will not go stale.
Highest blast radius: `IWytchInteractable` (3) — PROTECTED per DEC-009.

---

## Session State

**Session:** SES-004
**Token budget:** 128,000 total · ~3,500 estimated at boot from this file · ~124,500 remaining
**Active module:** `foreman_ai` — status: `in_progress`
**Active missions:** none
**Steps complete:** 2a ✓ · 2b ✓ · 2c ✓
**Last backup:** 2026-03-08T23:09:32Z — clean (6 files)

---

## Steps This Session: 2d + 2e

Both touch the same file — do them together.

### Step 2d: `HasIdleWorkers` — replace tag query with roster + `IWytchCommandable`

**File:** `Source/TheWytching/ForemanStateTreeConditions.cpp`

**Current (tag-based — dead end DE-004):**
```cpp
bool FForemanCondition_HasIdleWorkers::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
    APawn* Pawn = Data.Pawn.Get();
    if (!Pawn || !Pawn->GetWorld()) return false;

    TArray<AActor*> Workers;
    UGameplayStatics::GetAllActorsWithTag(Pawn->GetWorld(), FName(TEXT("Worker")), Workers);
    for (AActor* Worker : Workers)
    {
        if (Worker && Worker->Tags.Contains(FName(TEXT("Idle"))))
            return true;
    }
    // TODO: Replace with BPI_WorkerCommand::GetWorkerState()
    return false;
}
```

**Replace with:**
```cpp
bool FForemanCondition_HasIdleWorkers::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
    APawn* Pawn = Data.Pawn.Get();
    if (!Pawn) return false;

    AForeman_AIController* ForemanAIC = Cast<AForeman_AIController>(Pawn->GetController());
    if (!ForemanAIC) return false;

    for (const TWeakObjectPtr<AActor>& WeakWorker : ForemanAIC->GetRegisteredWorkers())
    {
        AActor* Worker = WeakWorker.Get();
        if (!Worker) continue;
        if (IWytchCommandable* Commandable = Cast<IWytchCommandable>(Worker))
        {
            if (Commandable->Execute_GetWorkerState(Worker) == EWorkerState::Idle)
                return true;
        }
    }
    return false;
}
```

**New includes needed (add if not present):**
```cpp
#include "Foreman_AIController.h"
#include "IWytchCommandable.h"
```
Remove `#include "Kismet/GameplayStatics.h"` if it becomes unused after 2e.

---

### Step 2e: `HasAvailableWork` — replace tag query with SmartObject subsystem

**File:** `Source/TheWytching/ForemanStateTreeConditions.cpp`

**What to add/replace:** Find the `FForemanCondition_HasAvailableWork::TestCondition` implementation and replace its body.

**Replace with:**
```cpp
bool FForemanCondition_HasAvailableWork::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
    APawn* Pawn = Data.Pawn.Get();
    if (!Pawn || !Pawn->GetWorld()) return false;

    USmartObjectSubsystem* SOSubsystem = USmartObjectSubsystem::GetCurrent(Pawn->GetWorld());
    if (!SOSubsystem) return false;

    FSmartObjectRequest Request;
    Request.QueryBox = FBox::BuildAABB(Pawn->GetActorLocation(), FVector(5000.f));

    TArray<FSmartObjectRequestResult> Results;
    SOSubsystem->FindSmartObjects(Request, Results);

    for (const FSmartObjectRequestResult& Result : Results)
    {
        if (SOSubsystem->GetSlotState(Result.SlotHandle) == ESmartObjectSlotState::Free)
            return true;
    }
    return false;
}
```

**New includes needed (add if not present):**
```cpp
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
```

---

## Current Source State (as of SES-003 end)

### `ForemanStateTreeEvaluators.cpp` — fully updated (Step 2c ✓)
ScanWorld() now uses:
- `ForemanAIC->GetRegisteredWorkers()` + `IWytchCommandable::Execute_GetWorkerState()`
- `USmartObjectSubsystem::FindSmartObjects()` with `FBox::BuildAABB(location, 5000u)`
- `ESmartObjectSlotState::Free` for available count
- `GetController()` cast to `AForeman_AIController*` with warning on failure

Includes in evaluators.cpp:
```
ForemanTypes.h / Foreman_AIController.h / IWytchCommandable.h
AndroidConditionComponent.h / SmartObjectSubsystem.h / SmartObjectRequestTypes.h
GameFramework/Pawn.h
```

### `Foreman_AIController.h/.cpp` — Step 2b ✓
- `GetOwnCondition()` added, forward decl + include + impl all clean.
- `GetRegisteredWorkers()` returns `const TArray<TWeakObjectPtr<AActor>>&` — C++ only.

### `ForemanStateTreeConditions.cpp` — NOT YET UPDATED
- `HasIdleWorkers`: still tag-based (Steps 2d does this)
- `HasAvailableWork`: unknown — check file before assuming. May be stub or tag-based.

---

## Relevant Decisions (pre-checked)

- **DEC-003:** Foreman mediates all assignments.
- **DEC-004:** Worker holds the SmartObject claim. Foreman identifies the match and sends the handle.
- **DEC-005:** All AI and controllers in C++. Blueprint only for animations, designer-tweakable vars, visual debug.
- **DEC-009:** `IWytchInteractable` PROTECTED. Not touched in Steps 2d–2e.

## Relevant Dead Ends (pre-checked)

- **DE-004:** `GetAllActorsWithTag` for worker/work queries — confirmed dead end. Roster + SmartObject subsystem are the replacement.

---

## After Steps 2d+2e — What Comes Next

| Step | File | Change |
|---|---|---|
| 2f | `ForemanStateTreeTasks.cpp` | `PlanJob`: SmartObject query + capability tag matching |
| 2g | `ForemanStateTreeTasks.cpp` | `AssignWorker`: send via `IWytchCommandable` |

**Open question on 2f:** Search radius for SmartObject query in PlanJob — suggest 5000 units as default (matching evaluator), confirm with Ricky before writing.

---

## SmartObject API Cheatsheet (UE 5.7 — verified)

```cpp
USmartObjectSubsystem* SOSubsystem = USmartObjectSubsystem::GetCurrent(World);
FSmartObjectRequest Request;
Request.QueryBox = FBox::BuildAABB(Origin, FVector(Radius));
// Optional filter:
// Request.Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
TArray<FSmartObjectRequestResult> Results;
SOSubsystem->FindSmartObjects(Request, Results);
ESmartObjectSlotState State = SOSubsystem->GetSlotState(Result.SlotHandle);
// States: Free / Claimed / Occupied / Disabled
```

⚠️ `FSmartObjectRequestFilter::ActivityRequirements` is `FGameplayTagQuery` — NOT `FGameplayTagContainer`.
⚠️ Do NOT use deprecated `Claim()` / `Release()` — use `MarkSlotAsClaimed()` / `MarkSlotAsFree()`.

---

## Session End Protocol (when Ricky says wrap up)

1. Update `modules/foreman_ai/status.json` — mark changed files, update `last_updated`
2. Append to `modules/foreman_ai/session.jsonl`
3. Update `memory/working_memory.json` — next step, tokens consumed
4. Update `memory/session_summary.md` — append SES-004 block
5. Run `backup.ps1`
6. Write `COLD_START_SES005.md` for the next task

## Task Boundary Triggers (write cold start package + tell Ricky to open new window)
- Current step completes
- Module status changes
- Direction change from Ricky
- Tokens consumed > 40,000 in current window
Do not ask. Just do it.

---

*This file may be deleted after SES-004 boots successfully.*
*Aisling Brain System · TheWytching · Cold Start Package · Written 2026-03-08*

