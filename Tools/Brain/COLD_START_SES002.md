# TheWytching — Cold Start Package
### SES-002 · Step 2a · Aisling Brain System
`Written at end of SES-001. Load this file only. Do not re-read the spec.`

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
Highest blast radius: `IWytchInteractable` (3) — PROTECTED per DEC-009.

---

## Session State

**Session:** SES-002
**Token budget:** 128,000 total · ~4,500 estimated at boot from this file · ~123,500 remaining
**Active module:** `foreman_ai` — status: `in_progress`
**Active missions:** none — no change_set_id needed (Step 2a touches only foreman_ai)
**Last backup:** 2026-03-08T22:42:56Z — clean

---

## The One Task

**Step 2a:** Add worker roster to `AForeman_AIController`.

**Files to edit** (existing — Live Coding OK, no editor restart needed):
- `Source/TheWytching/Foreman_AIController.h`
- `Source/TheWytching/Foreman_AIController.cpp`

**What to add:**

```cpp
// Foreman_AIController.h — add to public section:
UFUNCTION(BlueprintCallable, Category = "Foreman|Workers")
void RegisterWorker(AActor* Worker);

UFUNCTION(BlueprintCallable, Category = "Foreman|Workers")
void UnregisterWorker(AActor* Worker);

UFUNCTION(BlueprintCallable, Category = "Foreman|Workers")
const TArray<TWeakObjectPtr<AActor>>& GetRegisteredWorkers() const { return RegisteredWorkers; }

// Foreman_AIController.h — add to protected section:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Foreman|Workers")
TArray<TWeakObjectPtr<AActor>> RegisteredWorkers;
```

```cpp
// Foreman_AIController.cpp — implementations:
void AForeman_AIController::RegisterWorker(AActor* Worker)
{
    if (!Worker) return;
    // Avoid duplicates
    for (const TWeakObjectPtr<AActor>& Entry : RegisteredWorkers)
    {
        if (Entry.Get() == Worker) return;
    }
    RegisteredWorkers.Add(Worker);
    UE_LOG(LogForeman, Log, TEXT("RegisterWorker: %s — roster size: %d"),
        *Worker->GetName(), RegisteredWorkers.Num());
}

void AForeman_AIController::UnregisterWorker(AActor* Worker)
{
    if (!Worker) return;
    const int32 Before = RegisteredWorkers.Num();
    RegisteredWorkers.RemoveAll([Worker](const TWeakObjectPtr<AActor>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Worker;
    });
    UE_LOG(LogForeman, Log, TEXT("UnregisterWorker: %s — roster size: %d -> %d"),
        *Worker->GetName(), Before, RegisteredWorkers.Num());
}
```

**Why this design:**
- `TWeakObjectPtr` — workers can be destroyed without crashing the Foreman (DEC-004: worker holds claim, not Foreman — this pattern is consistent)
- `RemoveAll` in `UnregisterWorker` also purges any other stale nulls in the same pass — free cleanup
- `UPROPERTY` on the array — GC-safe, Blueprint-readable for debug

---

## Relevant Decisions (pre-checked — do not re-read decisions.json for this task)

- **DEC-003:** Foreman mediates all assignments. Workers do NOT autonomously find SmartObjects.
- **DEC-004:** Worker holds the SmartObject claim. Foreman identifies the match and sends the handle.
- **DEC-005:** All AI and controllers in C++. Blueprint only for animations, designer-tweakable vars, visual debug.
- **DEC-009:** `IWytchInteractable` PROTECTED. Blast radius 3. Not touched in Step 2a — no concern.

---

## Relevant Dead Ends (pre-checked — do not re-read dead_ends.json for this task)

- **DE-004:** `GetAllActorsWithTag` for worker/work queries — confirmed dead end. Step 2 replaces all stubs. The roster is the first part of that replacement.

---

## Current Source State (read at SES-001 end)

`Foreman_AIController.h` — 40 lines. No roster. No `GetOwnCondition`. Clean existing structure:
```cpp
UCLASS()
class THEWYTCHING_API AForeman_AIController : public AAIController
{
    GENERATED_BODY()
public:
    AForeman_AIController();
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    void ExecuteMoveToLocation(FVector Destination);
    void ExecuteMoveToActor(AActor* Target);
    UFUNCTION(BlueprintCallable, Category = "Foreman")
    UForeman_BrainComponent* GetForemanBrain() const { return ForemanBrain; }
protected:
    TObjectPtr<UStateTreeAIComponent> StateTreeAI;
    TObjectPtr<UStateTree> DefaultStateTree;
    TObjectPtr<UForeman_BrainComponent> ForemanBrain;
};
```

`IWytchCommandable.h` — key signatures for Steps 2c–2g reference:
```cpp
void ReceiveSmartObjectAssignment(FSmartObjectClaimHandle, FSmartObjectSlotHandle, AActor*);
EWorkerState GetWorkerState() const;
FGameplayTagContainer GetCapabilities() const;
void AbortCurrentTask(EAbortReason Reason);
```

---

## After Step 2a — What Comes Next

| Step | File | Change |
|---|---|---|
| 2b | `Foreman_AIController.h/.cpp` | Add `GetOwnCondition()` → `UAndroidConditionComponent*` |
| 2c | `ForemanStateTreeEvaluators.cpp` | `ScanWorld()`: roster + `GetWorkerState()` + `FindSmartObjects()` |
| 2d | `ForemanStateTreeConditions.cpp` | `HasIdleWorkers`: roster query |
| 2e | `ForemanStateTreeConditions.cpp` | `HasAvailableWork`: SmartObject subsystem |
| 2f | `ForemanStateTreeTasks.cpp` | `PlanJob`: SmartObject query + capability tag matching |
| 2g | `ForemanStateTreeTasks.cpp` | `AssignWorker`: send via `IWytchCommandable` |

**Open question on 2f:** Search radius for SmartObject query in PlanJob — suggest 5000 units as default, confirm with Ricky before writing.

---

## Session End Protocol (when Ricky says wrap up)

1. Update `modules/foreman_ai/status.json` — mark changed files, update `last_updated`
2. Append to `modules/foreman_ai/session.jsonl`
3. Update `memory/working_memory.json` — next step, tokens consumed
4. Update `memory/session_summary.md` — append SES-002 block
5. Run `backup.ps1`
6. Write `COLD_START_SES003.md` for the next task

## Task Boundary Triggers (write cold start package + tell Ricky to open new window)
- Current step completes (e.g. 2a done → write package for 2b before continuing)
- Module status changes
- Direction change from Ricky
- Tokens consumed > 40,000 in current window
Do not ask. Just do it. Write the package, run backup, say: "Task boundary — open new window, load COLD_START_SES00N.md."

---

*This file may be deleted after SES-002 boots successfully.*
*Aisling Brain System · TheWytching · Cold Start Package · Written 2026-03-08*

