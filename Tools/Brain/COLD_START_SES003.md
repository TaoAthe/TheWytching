````markdown
# TheWytching — Cold Start Package
### SES-003 · Step 2b · Aisling Brain System
`Written at end of SES-002. Load this file only. Do not re-read the spec.`

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
Step 2b only touches `Foreman_AIController.h/.cpp` — no new classes, hierarchy will not go stale.
Highest blast radius: `IWytchInteractable` (3) — PROTECTED per DEC-009.

---

## Session State

**Session:** SES-003
**Token budget:** 128,000 total · ~3,500 estimated at boot from this file · ~124,500 remaining
**Active module:** `foreman_ai` — status: `in_progress`
**Active missions:** none
**Steps complete:** 2a ✓
**Last backup:** 2026-03-08T23:01:14Z — clean (4 files)

---

## The One Task

**Step 2b:** Add `GetOwnCondition()` to `AForeman_AIController`.

**Files to edit** (existing — Live Coding OK, no editor restart needed):
- `Source/TheWytching/Foreman_AIController.h`
- `Source/TheWytching/Foreman_AIController.cpp`

**What to add:**

```cpp
// Foreman_AIController.h — add to public section (after GetForemanBrain):
UFUNCTION(BlueprintCallable, Category = "Foreman")
UAndroidConditionComponent* GetOwnCondition() const;
```

```cpp
// Foreman_AIController.h — add forward declaration at top (with other class forward decls):
class UAndroidConditionComponent;
```

```cpp
// Foreman_AIController.cpp — add include near top:
#include "AndroidConditionComponent.h"

// Add implementation:
UAndroidConditionComponent* AForeman_AIController::GetOwnCondition() const
{
    if (APawn* Pawn = GetPawn())
    {
        return Pawn->FindComponentByClass<UAndroidConditionComponent>();
    }
    return nullptr;
}
```

**Why this design:**
- Foreman is a Controller — it doesn't own the component, its Pawn does. `FindComponentByClass` on `GetPawn()` is the correct pattern.
- Null guard on `GetPawn()` — controller may not be possessing at call time.
- Used by Step 2c `ScanWorld()` evaluator to read the Foreman's own condition state before dispatching work.

---

## Current Source State (as of SES-002 end)

`Foreman_AIController.h` — 53 lines. Roster added. Structure:
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
    // Worker Roster (added 2a):
    UFUNCTION(BlueprintCallable, Category = "Foreman|Workers")
    void RegisterWorker(AActor* Worker);
    UFUNCTION(BlueprintCallable, Category = "Foreman|Workers")
    void UnregisterWorker(AActor* Worker);
    const TArray<TWeakObjectPtr<AActor>>& GetRegisteredWorkers() const { return RegisteredWorkers; }
protected:
    TObjectPtr<UStateTreeAIComponent> StateTreeAI;
    TObjectPtr<UStateTree> DefaultStateTree;
    TObjectPtr<UForeman_BrainComponent> ForemanBrain;
    TArray<TWeakObjectPtr<AActor>> RegisteredWorkers;  // VisibleAnywhere, no BlueprintReadOnly
};
```

**NOTE on TWeakObjectPtr:** UHT does not support `TArray<TWeakObjectPtr<>>` as Blueprint-exposed. `GetRegisteredWorkers` is C++ only. `RegisteredWorkers` UPROPERTY has no `BlueprintReadOnly`. This is correct — document if questioned.

---

## Relevant Decisions (pre-checked)

- **DEC-003:** Foreman mediates all assignments.
- **DEC-004:** Worker holds the SmartObject claim. Foreman identifies the match and sends the handle.
- **DEC-005:** All AI and controllers in C++. Blueprint only for animations, designer-tweakable vars, visual debug.
- **DEC-009:** `IWytchInteractable` PROTECTED. Not touched in Step 2b.

---

## Relevant Dead Ends (pre-checked)

- **DE-004:** `GetAllActorsWithTag` for worker/work queries — confirmed dead end. Roster (Step 2a) + SmartObject subsystem (Steps 2e–2f) are the replacement.

---

## After Step 2b — What Comes Next

| Step | File | Change |
|---|---|---|
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
4. Update `memory/session_summary.md` — append SES-003 block
5. Run `backup.ps1`
6. Write `COLD_START_SES004.md` for the next task

## Task Boundary Triggers (write cold start package + tell Ricky to open new window)
- Current step completes (e.g. 2b done → write package for 2c before continuing)
- Module status changes
- Direction change from Ricky
- Tokens consumed > 40,000 in current window
Do not ask. Just do it. Write the package, run backup, say: "Task boundary — open new window, load COLD_START_SES00N.md."

---

*This file may be deleted after SES-003 boots successfully.*
*Aisling Brain System · TheWytching · Cold Start Package · Written 2026-03-08*
````

