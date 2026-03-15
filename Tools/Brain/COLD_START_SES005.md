# TheWytching — Cold Start Package
### SES-005 · Steps 2f + 2g · Aisling Brain System
`Written at end of SES-004. Load this file only. Do not re-read the spec.`

---

## Who You Are

You are Aisling (Rider). C++ surface. JetBrains Rider. TheWytching project.
Ricky is Senior Foreman — sole authority on architectural decisions.
Aisling_BP is live in the Unreal Editor — bridge is `Tools/Brain/foreman/handshake.json`.
Global rules: `C:\AislingBrain\_core\global_rules.md` (v2.5 — key rules summarised below).

---

## Mandatory Rules

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
**Check staleness at boot** — compare timestamp against newest `.h` or `.cpp` in `Source/`.
Steps 2f+2g only modify `ForemanStateTreeTasks.cpp` — no new classes. Hierarchy will not go stale.
Highest blast radius: `IWytchInteractable` (3) — PROTECTED per DEC-009. Not touched in 2f+2g.

---

## Session State

**Session:** SES-005
**Token budget:** 128,000 total · ~3,500 estimated at boot · ~124,500 remaining
**Active module:** `foreman_ai` — status: `in_progress`
**Steps complete:** 2a ✓ · 2b ✓ · 2c ✓ · 2d ✓ · 2e ✓
**Last backup:** 2026-03-08T23:13:30Z — clean
**DE-004 status:** Fully retired from evaluator + conditions. Tasks file is the last remaining tag-based code.

---

## Target File

**`Source/TheWytching/ForemanStateTreeTasks.cpp`** — read it before making changes.

Also read: `ForemanStateTreeTasks.h` — understand the instance data structs before editing the .cpp.

---

## Step 2f: `PlanJob` — SmartObject subsystem query + capability matching

### What to replace

Find `FForemanTask_PlanJob::EnterState` in `ForemanStateTreeTasks.cpp`.

Current behavior (tag-based):
- `GetAllActorsWithTag(World, "WorkStation", WorkStations)` to find targets
- `GetAllActorsWithTag(World, "Worker", Workers)` to find workers
- Tag-based idle check: `Worker->Tags.Contains("Idle")`
- Tag-based claim: `Target->Tags.AddUnique("Claimed")`

**Replace with:**

```cpp
EStateTreeRunStatus FForemanTask_PlanJob::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

    APawn* Pawn = Data.Pawn.Get();
    if (!Pawn || !Pawn->GetWorld())
        return EStateTreeRunStatus::Failed;

    UWorld* World = Pawn->GetWorld();

    // ── 1. Find available SmartObject slots ──
    USmartObjectSubsystem* SOSubsystem = USmartObjectSubsystem::GetCurrent(World);
    if (!SOSubsystem)
    {
        UE_LOG(LogForeman, Warning, TEXT("PlanJob: SmartObjectSubsystem unavailable"));
        return EStateTreeRunStatus::Failed;
    }

    FSmartObjectRequest Request;
    Request.QueryBox = FBox::BuildAABB(Pawn->GetActorLocation(), FVector(5000.f));
    TArray<FSmartObjectRequestResult> Results;
    SOSubsystem->FindSmartObjects(Request, Results);

    // Pick closest Free slot
    FSmartObjectRequestResult BestResult;
    float BestDistSq = FLT_MAX;
    bool bFoundSlot = false;

    for (const FSmartObjectRequestResult& Result : Results)
    {
        if (SOSubsystem->GetSlotState(Result.SlotHandle) != ESmartObjectSlotState::Free)
            continue;

        TOptional<FTransform> SlotTransform = SOSubsystem->GetSlotTransform(Result);
        if (!SlotTransform.IsSet()) continue;

        const float DistSq = FVector::DistSquared(
            Pawn->GetActorLocation(), SlotTransform.GetValue().GetLocation());
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestResult = Result;
            bFoundSlot = true;
        }
    }

    if (!bFoundSlot)
    {
        UE_LOG(LogForeman, Log, TEXT("PlanJob: No free SmartObject slots found"));
        return EStateTreeRunStatus::Failed;
    }

    // ── 2. Find best idle worker from roster ──
    AForeman_AIController* ForemanAIC = Cast<AForeman_AIController>(Pawn->GetController());
    if (!ForemanAIC)
    {
        UE_LOG(LogForeman, Warning, TEXT("PlanJob: No AForeman_AIController on pawn"));
        return EStateTreeRunStatus::Failed;
    }

    AActor* BestWorker = nullptr;
    for (const TWeakObjectPtr<AActor>& WeakWorker : ForemanAIC->GetRegisteredWorkers())
    {
        AActor* Worker = WeakWorker.Get();
        if (!Worker) continue;
        if (IWytchCommandable* Commandable = Cast<IWytchCommandable>(Worker))
        {
            if (Commandable->Execute_GetWorkerState(Worker) == EWorkerState::Idle)
            {
                BestWorker = Worker;
                break;  // First idle worker — capability matching added in Step 3
            }
        }
    }

    if (!BestWorker)
    {
        UE_LOG(LogForeman, Log, TEXT("PlanJob: No idle workers in roster"));
        return EStateTreeRunStatus::Failed;
    }

    // ── 3. Store result in InstanceData for AssignWorker task ──
    Data.SelectedSlotResult = BestResult;
    Data.SelectedWorker = BestWorker;

    UE_LOG(LogForeman, Log, TEXT("PlanJob: Planned job — worker: %s"),
        *BestWorker->GetName());

    return EStateTreeRunStatus::Succeeded;
}
```

**New includes needed in `ForemanStateTreeTasks.cpp`:**
```cpp
#include "Foreman_AIController.h"
#include "IWytchCommandable.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
```

**Instance data changes:** Check `FForemanTask_PlanJobInstanceData` in the .h file. It likely has `SelectedWorker` and some form of target storage already. If `SelectedSlotResult` (`FSmartObjectRequestResult`) doesn't exist yet, add it to the struct — or use whatever target field already exists. Adapt to what's already there. Do NOT add new UPROPERTYs without checking if they already exist.

---

## Step 2g: `AssignWorker` — send via `IWytchCommandable::ReceiveSmartObjectAssignment`

### What to replace

Find `FForemanTask_AssignWorker::EnterState` in `ForemanStateTreeTasks.cpp`.

Current behavior (tag-based):
- Tags `Target` as `"Claimed"`
- Removes `"Idle"` tag from `Worker`
- Calls `Execute_ReceiveCommand_Implementation` or similar Blueprint command

**Replace with:**

```cpp
EStateTreeRunStatus FForemanTask_AssignWorker::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

    APawn* Pawn = Data.Pawn.Get();
    AActor* Worker = Data.SelectedWorker.Get();
    if (!Pawn || !Worker || !Pawn->GetWorld())
    {
        UE_LOG(LogForeman, Warning, TEXT("AssignWorker: Missing pawn or worker"));
        return EStateTreeRunStatus::Failed;
    }

    USmartObjectSubsystem* SOSubsystem = USmartObjectSubsystem::GetCurrent(Pawn->GetWorld());
    if (!SOSubsystem)
    {
        UE_LOG(LogForeman, Warning, TEXT("AssignWorker: SmartObjectSubsystem unavailable"));
        return EStateTreeRunStatus::Failed;
    }

    // Notify the worker — worker will claim slot on arrival per DEC-004
    if (IWytchCommandable* Commandable = Cast<IWytchCommandable>(Worker))
    {
        // Pass an invalid ClaimHandle — worker claims on arrival (DEC-004)
        Commandable->Execute_ReceiveSmartObjectAssignment(
            Worker,
            FSmartObjectClaimHandle::InvalidHandle,
            Data.SelectedSlotResult.SlotHandle,
            nullptr);  // TargetActor — optional, worker resolves from SlotHandle

        UE_LOG(LogForeman, Log, TEXT("AssignWorker: Sent assignment to %s"),
            *Worker->GetName());
        return EStateTreeRunStatus::Succeeded;
    }

    UE_LOG(LogForeman, Warning, TEXT("AssignWorker: Worker %s does not implement IWytchCommandable"),
        *Worker->GetName());
    return EStateTreeRunStatus::Failed;
}
```

---

## Pre-flight Checks Before Editing

1. **Read `ForemanStateTreeTasks.h`** — understand all instance data structs (PlanJob + AssignWorker). Adapt the implementation to what's actually there.
2. **Read `ForemanStateTreeTasks.cpp` fully** — know the existing implementation before replacing. Tag-based code may differ from what's shown above.
3. **Verify `FSmartObjectClaimHandle::InvalidHandle`** exists in UE 5.7 — search engine source if uncertain. If it doesn't exist, construct default `FSmartObjectClaimHandle{}`.
4. **Check `GetSlotTransform` signature** — in UE 5.7 it may be `GetSlotTransform(FSmartObjectSlotHandle)` or `GetSlotTransform(FSmartObjectRequestResult)`. Confirm before writing.

---

## Current Source State (as of SES-004 end — all confirmed clean)

| File | Status |
|---|---|
| `Foreman_AIController.h/.cpp` | ✓ Step 2b — GetOwnCondition(), GetRegisteredWorkers() |
| `ForemanStateTreeEvaluators.cpp` | ✓ Step 2c — ScanWorld() on SmartObject + roster |
| `ForemanStateTreeConditions.cpp` | ✓ Steps 2d+2e — HasIdleWorkers + HasAvailableWork on SmartObject + roster |
| `ForemanStateTreeTasks.cpp` | ⚠️ Steps 2f+2g — still tag-based. **Target for this session.** |

---

## Relevant Decisions (pre-checked)

- **DEC-003:** Foreman mediates all assignments. Worker does NOT auto-find SmartObjects.
- **DEC-004:** Worker holds the claim, not the Foreman. Foreman identifies the slot, sends the worker slot info, worker claims on arrival.
- **DEC-005:** All AI and controllers in C++. Blueprint only for animations/designer vars.
- **DEC-009:** `IWytchInteractable` PROTECTED. Not touched in 2f+2g.
- **DEC-010:** SmartObject subsystem queries are authoritative ground truth. `GetAllActorsWithTag` is a dead end (DE-004) — must not be added or re-added anywhere, including tasks.

## Relevant Dead Ends (pre-checked)

- **DE-004:** `GetAllActorsWithTag` for worker/work queries — confirmed dead end. Fully retired after 2f+2g.

---

## SmartObject API Cheatsheet (UE 5.7 — verified)

```cpp
USmartObjectSubsystem* SOSubsystem = USmartObjectSubsystem::GetCurrent(World);

// Query
FSmartObjectRequest Request;
Request.QueryBox = FBox::BuildAABB(Origin, FVector(Radius));
// Optional filter:
// Request.Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
TArray<FSmartObjectRequestResult> Results;
SOSubsystem->FindSmartObjects(Request, Results);

// Slot state
ESmartObjectSlotState State = SOSubsystem->GetSlotState(Result.SlotHandle);
// States: Free / Claimed / Occupied / Disabled

// Slot location (for navigation targeting)
TOptional<FTransform> T = SOSubsystem->GetSlotTransform(Result);

// Claim (worker calls this, not Foreman — DEC-004)
FSmartObjectClaimHandle Handle = SOSubsystem->MarkSlotAsClaimed(SlotHandle, Priority, UserData);
// Release
SOSubsystem->MarkSlotAsFree(Handle);
```

⚠️ `FSmartObjectRequestFilter::ActivityRequirements` is `FGameplayTagQuery` — NOT `FGameplayTagContainer`.
⚠️ Do NOT use deprecated `Claim()` / `Release()`.
⚠️ Do NOT pre-claim slots in PlanJob — the Foreman only identifies the slot, worker claims (DEC-004).

---

## After Steps 2f+2g — What Comes Next

| Step | Scope | Notes |
|---|---|---|
| 3 | Worker system | Workers implement `IWytchCommandable` in C++. `BP_AutoBot` first. |
| 4 | Monitor task | `FForemanTask_Monitor` polls active assignments via SmartObject subsystem |
| 5 | Triage state | New state: handle critical worker conditions mid-assignment |

**Open question before Step 3:** Which worker actor class implements `IWytchCommandable` first? `BP_AutoBot` is the candidate (already has partial BPI_WorkerCommand). Confirm with Ricky.

---

## Session End Protocol (when Ricky says wrap up)

1. Update `modules/foreman_ai/status.json`
2. Append to `modules/foreman_ai/session.jsonl`
3. Update `memory/working_memory.json`
4. Update `memory/session_summary.md` — append SES-005 block
5. Run `backup.ps1`
6. Write `COLD_START_SES006.md`

## Task Boundary Triggers
- Current step completes
- Module status changes
- Direction change from Ricky
- Tokens > 40,000 consumed
Do not ask. Just do it.

---

*This file may be deleted after SES-005 boots successfully.*
*Aisling Brain System · TheWytching · Cold Start Package · Written 2026-03-08*

