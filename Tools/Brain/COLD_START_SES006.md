````markdown
# TheWytching — Cold Start Package
### SES-006 · Step 3 · Worker IWytchCommandable Implementation
`Written at end of SES-005. Load this file only. Do not re-read the spec.`

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
Step 3 will add `IWytchCommandable` implementations to worker classes — hierarchy may go stale if new classes are added or existing ones change base class. Regenerate if a new `.h` file is created.

---

## Session State

**Session:** SES-006
**Token budget:** 128,000 total · ~3,500 estimated at boot · ~124,500 remaining
**Active module:** `foreman_ai` + `worker_system` — status: `in_progress`
**Steps complete:** 2a ✓ · 2b ✓ · 2c ✓ · 2d ✓ · 2e ✓ · 2f ✓ · 2g ✓
**Last backup:** 2026-03-09T00:30:00Z — clean
**DE-004 status:** FULLY RETIRED. `GetAllActorsWithTag` removed from all Foreman AI files.

---

## Target for This Session: Step 3

**Workers implement `IWytchCommandable` in C++.**
`BP_AutoBot` is the first candidate — it already has a partial `BPI_WorkerCommand` Blueprint interface.

**Before starting Step 3:**
1. Confirm with Ricky which worker actor class goes first (`BP_AutoBot` or another)
2. Read `IWytchCommandable.h` — understand the full interface contract
3. Find the C++ class backing `BP_AutoBot` (likely `AAndroidPawn` or similar) — check `Source/`
4. Read the `worker_system` module status: `modules/worker_system/status.json`
5. Check `decisions.json` for DEC-003, DEC-004, DEC-005 — they govern how workers receive and hold assignments

**Interface to implement (from `IWytchCommandable.h`):**
```cpp
void ReceiveSmartObjectAssignment(FSmartObjectClaimHandle ClaimHandle, FSmartObjectSlotHandle SlotHandle, AActor* TargetActor);
EWorkerState GetWorkerState() const;
FGameplayTagContainer GetCapabilities() const;
void AbortCurrentTask(EAbortReason Reason);
```

**Key constraints:**
- DEC-003: Foreman mediates all assignments. Worker does NOT auto-find SmartObjects.
- DEC-004: Worker holds the claim. Worker claims the slot on arrival (not in ReceiveSmartObjectAssignment). Store SlotHandle, navigate, claim on BeginWork.
- DEC-005: All AI/controllers in C++. Blueprint only for animations/designer vars.
- `BPI_WorkerCommand` Blueprint interface on `BP_AutoBot` — check for orphaned graphs before adding C++ implementation. Deleting orphaned BP graph nodes may be needed (editor work — flag for Ricky if so).

---

## Current Source State (as of SES-005 end — all confirmed clean)

| File | Status |
|---|---|
| `Foreman_AIController.h/.cpp` | ✓ Step 2b — GetOwnCondition(), GetRegisteredWorkers() |
| `ForemanStateTreeEvaluators.cpp` | ✓ Step 2c — ScanWorld() on SmartObject + roster |
| `ForemanStateTreeConditions.cpp` | ✓ Steps 2d+2e — HasIdleWorkers + HasAvailableWork on SmartObject + roster |
| `ForemanStateTreeTasks.h` | ✓ Step 2f+2g — FSmartObjectRequestResult SelectedSlotResult in PlanJobInstanceData; AssignWorker uses PlanJobInstanceData |
| `ForemanStateTreeTasks.cpp` | ✓ Steps 2f+2g — SmartObject query + IWytchCommandable assignment. DE-004 fully retired. |
| Worker files | ⚠️ Step 3 — IWytchCommandable not yet implemented. **Target for this session.** |

---

## Key Architectural Notes for Step 3

**What PlanJob sends to AssignWorker (via shared InstanceData):**
- `Data.SelectedSlotResult` — `FSmartObjectRequestResult` (slot location, handle)
- `Data.SelectedWorker` — `TObjectPtr<AActor>` (the chosen idle worker)

**What AssignWorker sends to the worker:**
```cpp
Execute_ReceiveSmartObjectAssignment(
    Worker,
    FSmartObjectClaimHandle::InvalidHandle,  // Worker claims on arrival (DEC-004)
    Data.SelectedSlotResult.SlotHandle,      // Worker navigates here
    nullptr);                                // TargetActor — optional
```

**Worker's job after receiving the assignment:**
1. Store `SlotHandle` in internal state
2. Set `WorkerState = EWorkerState::Traveling`
3. Navigate to slot transform (query `USmartObjectSubsystem::GetSlotTransform(SlotHandle)`)
4. On arrival: call `SOSubsystem->MarkSlotAsClaimed(SlotHandle, ...)` → receive `FSmartObjectClaimHandle`
5. Execute work (BeginWork/TickWork/EndWork via `IWytchWorkSite` on the target actor)
6. On completion/failure: call `SOSubsystem->MarkSlotAsFree(ClaimHandle)`, set state to Idle

---

## Relevant Decisions (pre-checked)

- **DEC-003:** Foreman mediates all assignments. Worker does NOT auto-find SmartObjects.
- **DEC-004:** Worker holds the claim, not the Foreman. Worker claims on arrival.
- **DEC-005:** All AI and controllers in C++. Blueprint only for animations/designer vars.
- **DEC-009:** `IWytchInteractable` PROTECTED. Not touched in Step 3.
- **DEC-010:** SmartObject subsystem queries are authoritative. Tag-based queries are dead end (DE-004).

## Relevant Dead Ends (pre-checked)

- **DE-004:** `GetAllActorsWithTag` — fully retired after SES-005. Do not re-introduce.

---

## Open Question Before Starting

**Which worker actor class implements `IWytchCommandable` first?**
`BP_AutoBot` is the candidate — it has partial `BPI_WorkerCommand`. But confirm with Ricky:
- What is the C++ class that backs `BP_AutoBot`? (Check Content/Blueprints or Content/Characters)
- Does `BP_AutoBot` already have a C++ parent class, or is it a pure Blueprint?
- If pure Blueprint: Ricky decides whether to create a new C++ parent or implement IWytchCommandable directly on an existing class

Do NOT start implementing until Ricky confirms the target class.

---

## Session End Protocol (when Ricky says wrap up)

1. Update `modules/foreman_ai/status.json` + `modules/worker_system/status.json`
2. Append to `modules/foreman_ai/session.jsonl`
3. Update `memory/working_memory.json`
4. Update `memory/session_summary.md` — append SES-006 block
5. Run `backup.ps1`
6. Write `COLD_START_SES007.md`

## Task Boundary Triggers
- Current step completes
- Module status changes
- Direction change from Ricky
- Tokens > 40,000 consumed
Do not ask. Just do it.

---

*This file may be deleted after SES-006 boots successfully.*
*Aisling Brain System · TheWytching · Cold Start Package · Written 2026-03-09*
````

