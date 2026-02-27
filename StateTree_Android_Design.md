# Wytcherly — Android StateTree Architecture
## Foreman + Worker Autonomy, Degradation & Off-Duty Design

**Mapped to:** SmartObjects + IWytch\* + Capability Tags + Foreman-Mediated Dispatch  
**Engine:** UE 5.7 — C++ primary, Blueprint for animation/designer vars only  
**Goal:** Maximum perceived autonomy with minimum structural changes to existing architecture  
**Android doctrine:** Not sentient. Aware. No nourishment. Power + repair dependencies. Degradable subsystems.  
**Performance note:** All condition evaluation, tag manipulation, and state logic in C++. StateTrees tick natively in C++ already. No Blueprint AI logic.

---

## Part 1 — Android Condition Model

Before designing StateTrees, we need the data they read. Every android (Foreman included) carries a condition profile that drives StateTree transitions, capability filtering, and behavioral fallbacks.

### 1.1 — UAndroidConditionComponent (C++, on Character)

Lives on the **Character**, not the Controller — this is body state, not mind state. If the Controller is swapped or repossessed, the body's damage persists. Ticks on a slow timer (every 1–2s), not every frame.

```
UCLASS(ClassGroup=(Wytcherly), meta=(BlueprintSpawnableComponent))
class UAndroidConditionComponent : public UActorComponent

  // ── Power ──
  float PowerLevel;           // 0.0–1.0, drains over time based on activity
  float PowerDrainRate;       // Units/sec — idle is low, hauling is high, combat is highest
  float CriticalPowerThresh;  // Below this → forced return to charging station
  EAndroidPowerState PowerState;  // Normal, Low, Critical, Dead

  // ── Structural Integrity ──
  float StructuralHP;         // 0.0–1.0, damage from impacts/combat/environment
  float CriticalDamageThresh; // Below this → limping, reduced capability
  
  // ── Subsystem Status (bitfield or individual bools) ──
  ESubsystemStatus VisionStatus;      // Operational, Degraded, Destroyed
  ESubsystemStatus AudioStatus;       // Operational, Degraded, Destroyed
  ESubsystemStatus LocomotionStatus;  // Operational, Impaired, Immobile
  ESubsystemStatus ManipulatorLeft;   // Operational, Damaged, Destroyed
  ESubsystemStatus ManipulatorRight;  // Operational, Damaged, Destroyed
  ESubsystemStatus CommLink;          // Connected, Intermittent, Lost

  // ── Derived State ──
  EAndroidReadiness GetReadiness();   // Combat-ready, Work-ready, Degraded, NeedsRepair, Disabled
  FGameplayTagContainer GetCurrentCapabilities();  // Dynamic — removes tags as subsystems fail
```

**Key design principle:** Capabilities are *dynamic*. A General Worker with `Capability.Hauling` loses that tag when both manipulators are destroyed. The Foreman's SmartObject query naturally stops assigning that worker to hauling slots — no special-case code needed. The existing capability-matching architecture handles degradation for free.

### 1.2 — Subsystem Status Enum

```
UENUM(BlueprintType)
enum class ESubsystemStatus : uint8
{
    Operational,    // Full function
    Degraded,       // Partial function — accuracy/speed penalties
    Destroyed       // No function — capability tag removed
};
```

**Degraded** is where the interesting gameplay lives. A Degraded vision system means:
- Mud on lens → perception radius halved, periodic "wipe" animation plays (SmartObject: self-maintenance)
- Misalignment → targeting offset, navigation drift (subtle — walks slightly off-course)
- Partial damage → flickering perception (randomly loses/regains sight stimuli)

These don't remove the capability tag — the android can still work, just poorly. The Foreman might *choose* to reassign a degraded worker to less demanding tasks, but isn't forced to.

### 1.3 — Capability Tag Dynamics

Existing architecture uses static capability tags assigned at spawn. We extend this with a thin dynamic layer:

```
// On UAndroidConditionComponent
FGameplayTagContainer BaseCapabilities;      // Set at fabrication, never changes
FGameplayTagContainer ActiveCapabilities;    // Recalculated when subsystems change

void RecalculateCapabilities()
{
    ActiveCapabilities = BaseCapabilities;
    
    if (ManipulatorLeft == Destroyed && ManipulatorRight == Destroyed)
        ActiveCapabilities.RemoveTag(Tag_Capability_Hauling);
    
    if (LocomotionStatus == Destroyed)
        ActiveCapabilities.RemoveTag(Tag_Capability_Hauling);
        // Can't haul if you can't walk
    
    if (VisionStatus == Destroyed && AudioStatus == Destroyed)
        ActiveCapabilities.RemoveTag(Tag_Capability_Building);
        // Can't build blind and deaf
    
    if (LocomotionStatus == Impaired)
        ActiveCapabilities.RemoveTag(Tag_Capability_Combat);
        // Limping androids don't fight — they're a liability
    
    // Specialist removals
    if (ManipulatorRight == Destroyed)
        ActiveCapabilities.RemoveTag(Tag_Capability_LaserCutter);
        // Laser is on the right arm for AutoBots
}
```

**The Foreman's existing SmartObject query already filters by capability tags.** By making the worker's `GetCapabilities()` return `ActiveCapabilities` instead of `BaseCapabilities`, degradation flows through the entire dispatch system automatically. This is the zero-structural-change win.

### 1.4 — GameplayTag Extensions

Add to `DefaultGameplayTags.ini`:

```ini
# ── Android Status Tags (for StateTree conditions) ──
+GameplayTags=(Tag="Status.Power.Normal")
+GameplayTags=(Tag="Status.Power.Low")
+GameplayTags=(Tag="Status.Power.Critical")
+GameplayTags=(Tag="Status.Power.Dead")
+GameplayTags=(Tag="Status.NeedsRepair")
+GameplayTags=(Tag="Status.NeedsRecharge")
+GameplayTags=(Tag="Status.CommLink.Lost")
+GameplayTags=(Tag="Status.Vision.Degraded")
+GameplayTags=(Tag="Status.Vision.Destroyed")
+GameplayTags=(Tag="Status.Locomotion.Impaired")
+GameplayTags=(Tag="Status.Locomotion.Immobile")

# ── Activity Mode Tags ──
+GameplayTags=(Tag="Mode.OnDuty")
+GameplayTags=(Tag="Mode.OffDuty")
+GameplayTags=(Tag="Mode.Maintenance")
+GameplayTags=(Tag="Mode.Disabled")

# ── Task Extensions ──
+GameplayTags=(Tag="Task.Repair")
+GameplayTags=(Tag="Task.Recharge")
+GameplayTags=(Tag="Task.SelfMaintenance")
+GameplayTags=(Tag="Task.Recreation")

# ── SmartObject Slots for Infrastructure ──
+GameplayTags=(Tag="Task.Repair")       # Repair bay slots
+GameplayTags=(Tag="Task.Recharge")     # Charging station slots
+GameplayTags=(Tag="Task.Idle")         # Idle/recreation area slots
```

---

## Part 2 — Foreman StateTree

The Foreman is an android too. Same condition model, same degradation. But the Foreman has a dual-mode existence: **Boss Mode** (on-duty, dispatching workers) and **Android Mode** (off-duty, autonomous agent with personal needs).

### 2.1 — Foreman Root StateTree: `ST_Foreman`

```
ST_Foreman (Root)
│
├── [Evaluator] AndroidConditionEval        ← ticks every 0.5s, reads UAndroidConditionComponent
│       Outputs: PowerState, Readiness, CommLinkStatus, IsOnDuty
│
├── ★ STATE: Disabled                       ← PowerState == Dead OR Readiness == Disabled
│       Enter: broadcast "Foreman offline" to all workers
│       Task: PlayDeathAnimation / RagdollFreeze
│       Transition: none (requires external repair + repower to exit)
│
├── ★ STATE: EmergencySelfPreserve          ← PowerState == Critical OR StructuralHP < CriticalThresh
│       Priority: HIGHEST (interrupts anything)
│       Tasks:
│         1. AbortAllActiveDispatches()     ← tells workers to finish-or-drop current task
│         2. FindAndClaimSmartObject(Task.Recharge OR Task.Repair)
│         3. NavigateToSmartObject
│         4. InteractWithSmartObject (IWytchWorkSite → repair bay / charging station)
│       Transition → OnDuty when Readiness >= WorkReady AND PowerState >= Normal
│
├── ★ STATE: OnDuty                         ← IsOnDuty == true (default during work period)
│   │
│   ├── [Evaluator] WorkAvailabilityEval    ← existing evaluator, queries SmartObject subsystem
│   │
│   ├── STATE: Dispatch                     ← HasIdleWorkers AND HasAvailableWork
│   │     Task: FForemanTask_PlanJob        ← existing task (SmartObject query + capability match)
│   │           NEW: skip workers with Status.NeedsRepair or Status.Power.Critical
│   │           NEW: prefer workers with full capabilities over degraded ones
│   │     Task: FForemanTask_AssignWorker   ← existing task
│   │     Transition → Monitor
│   │
│   ├── STATE: Monitor                      ← has active assignments
│   │     Task: FForemanTask_Monitor        ← existing task (3s tick)
│   │           NEW: check worker condition changes mid-task
│   │           NEW: if worker goes Critical, mark task for reassignment
│   │     Transition → Dispatch when: worker completes/fails AND more work available
│   │     Transition → Idle when: no active assignments AND no available work
│   │
│   ├── STATE: Triage                       ← NEW: worker(s) reporting damage/low power
│   │     Task: EvaluateDamagedWorkers
│   │           - Sort by severity
│   │           - Assign repair bay SmartObjects to critical workers
│   │           - Reassign their pending tasks to healthy workers
│   │           - Workers with CommLink.Lost → send search party or flag for manual
│   │     Transition → Dispatch (always re-enters dispatch loop after triage)
│   │
│   ├── STATE: Rally                        ← existing state, unchanged
│   │     Task: FForemanTask_Rally
│   │
│   └── STATE: ForemanIdle                  ← no work, no alerts
│         Task: FForemanTask_Wait           ← existing (optional LLM scan)
│         NEW: Wander within work zone, observe workers (sells "boss walking the floor")
│         Transition → Dispatch when work becomes available
│         Transition → OffDuty when shift ends
│
└── ★ STATE: OffDuty                        ← IsOnDuty == false (shift ended, recreation period)
    │
    │   The Foreman stops being The Boss. Becomes just another android.
    │   Uses the SAME autonomous behavior as workers (see Part 3).
    │   Cannot be interrupted by work requests (except emergencies).
    │
    ├── STATE: PersonalMaintenance           ← self-check: do I need repairs/power?
    │     Task: FindAndClaimSmartObject(Task.Recharge / Task.Repair / Task.SelfMaintenance)
    │     Transition → Recreation when: all subsystems nominal, power adequate
    │
    ├── STATE: Recreation                    ← "downtime" behavior
    │     Task: AutonomousWander             ← explore, inspect surroundings
    │     Task: SocialProximity              ← move near other off-duty androids (not talking —
    │                                           just proximity, shared space. Androids aren't sentient
    │                                           but awareness creates patterns)
    │     Task: ObserveEnvironment           ← stop, look around, track moving things
    │     Transition → PersonalMaintenance when: power dips or damage detected
    │     Transition → OnDuty when: shift starts
    │
    └── STATE: EmergencyRecall               ← critical event during off-duty
          Condition: base under attack, critical infrastructure failure, etc.
          Task: snap to OnDuty immediately
          Transition → OnDuty
```

### 2.2 — What Changes In Existing Code

| Existing Asset | Change | Scope |
|---|---|---|
| `ST_Foreman` | Add root-level Disabled, EmergencySelfPreserve, OffDuty states. Add Triage child under OnDuty. Existing Dispatch/Monitor/Rally/Idle states stay intact inside OnDuty. | Medium — restructure but preserve inner logic |
| `FForemanTask_PlanJob` | Add capability check against `ActiveCapabilities` instead of `BaseCapabilities`. Add readiness filter (skip workers with NeedsRepair/Critical). | Small — two additional filter conditions |
| `FForemanTask_Monitor` | Add condition-change detection for active workers. If worker degrades mid-task, flag for Triage. | Small — add check to existing 3s tick |
| `FForemanEval_WorkAvailability` | Also output a `bHasDamagedWorkers` bool for Triage transition. | Tiny — one additional query |
| `AIC_Foreman` | Read `UAndroidConditionComponent` from possessed pawn. Expose to StateTree context. | Small |
| `DefaultGameplayTags.ini` | Add Status.\*, Mode.\*, Task.Repair/Recharge/etc. | Tiny |

No new interfaces. No new component types beyond `UAndroidConditionComponent`. No changes to SmartObject architecture. No changes to IWytch\* interfaces (repair bays and charging stations just implement `IWytchWorkSite` like everything else).

---

## Part 3 — Worker StateTree

Workers are simpler than the Foreman but need to feel alive. The key insight: **workers have two masters** — the Foreman (external commands) and their own condition (internal needs). The StateTree must arbitrate between them.

### 3.1 — Worker Root StateTree: `ST_Worker`

```
ST_Worker (Root)
│
├── [Evaluator] AndroidConditionEval        ← same evaluator as Foreman, shared C++ class
│       Outputs: PowerState, Readiness, LocomotionStatus, CommLinkStatus, IsOnDuty
│
├── ★ STATE: Disabled                       ← PowerState == Dead OR Readiness == Disabled
│       Enter: Release any held SmartObject claims
│       Enter: Report status to Foreman (if CommLink active) → Foreman removes from roster
│       Task: ShutdownAnimation → freeze / ragdoll
│       Exit: only via external repair event
│
├── ★ STATE: EmergencySelfPreserve          ← PowerState == Critical
│       Priority: overrides Commanded state
│       Enter: Release current SmartObject claim
│       Enter: Report "going critical" to Foreman (if CommLink active)
│       Tasks:
│         1. FindAndClaimSmartObject(Task.Recharge)    ← worker finds its OWN charging station
│         2. NavigateToSmartObject                      ← yes, this breaks Foreman-mediated pattern
│         3. InteractWithSmartObject                       ON PURPOSE — survival trumps hierarchy
│       Transition → OnDuty_Idle when PowerState >= Normal
│       NOTE: This is the ONE case where a worker claims its own SmartObject.
│             The Foreman is informed but does not mediate. Self-preservation is
│             hardcoded, not commanded.
│
├── ★ STATE: CommLinkLost                   ← CommLink == Lost
│   │
│   │   Worker cannot receive Foreman commands. Must act autonomously.
│   │   This is the "lost android" state — eerie, interesting gameplay.
│   │
│   ├── STATE: ReturnToBase                  ← try to navigate to last known base location
│   │     Task: NavigateToLocation(LastKnownBasePos)
│   │     Fallback: if navigation fails → Wander
│   │
│   ├── STATE: Wander                        ← can't reach base, wander and periodically retry comm
│   │     Task: AutonomousWander(radius limited)
│   │     Task: PeriodicCommRetry(every 30s)
│   │     Transition → ReturnToBase if comm restored
│   │
│   └── STATE: AutonomousWork               ← OPTIONAL, advanced: worker finds visible SmartObjects
│         Condition: can see a SmartObject matching own capabilities
│         Task: claim and work it alone (no Foreman mediation)
│         NOTE: This is genuinely autonomous behavior. The worker reverts to
│               standard UE5 "agent finds own SmartObject" pattern when severed
│               from the hierarchy. Beautiful fallback.
│
├── ★ STATE: OnDuty                         ← IsOnDuty == true AND CommLink active AND Readiness >= WorkReady
│   │
│   ├── STATE: Idle                          ← waiting for Foreman command
│   │     Tasks (concurrent, low-priority):
│   │       - IdleFidget                     ← subtle idle animations, weight shifts, look-around
│   │       - SelfCheck                      ← periodic self-diagnostic (drives "wipe lens" etc.)
│   │       - ProximityAwareness             ← track nearby actors, orient toward activity
│   │     Transition → Commanded when: ReceiveCommand from Foreman
│   │     Transition → SelfMaintenance when: minor degradation detected AND idle
│   │
│   ├── STATE: Commanded                     ← Foreman has given an assignment
│   │   │
│   │   ├── STATE: NavigatingToTask
│   │   │     Task: NavigateToSmartObject(ClaimHandle)
│   │   │     Transition → Working on arrival
│   │   │     Transition → ReportFailure if navigation fails (blocked, destroyed path)
│   │   │
│   │   ├── STATE: Working
│   │   │     Task: InteractWithSmartObject   ← calls IWytch* on target
│   │   │     Monitor: condition checks continue during work
│   │   │       - Power drops to Low → speed penalty, report to Foreman
│   │   │       - Manipulator damaged → work speed reduction, report
│   │   │       - Power drops to Critical → ABORT → EmergencySelfPreserve
│   │   │     Transition → ReportComplete on success
│   │   │     Transition → ReportFailure on inability to complete
│   │   │
│   │   ├── STATE: ReportComplete
│   │   │     Task: Release SmartObject claim
│   │   │     Task: Notify Foreman (task done, current condition)
│   │   │     Transition → Idle
│   │   │
│   │   └── STATE: ReportFailure
│   │         Task: Release SmartObject claim (if held)
│   │         Task: Notify Foreman (reason for failure, current condition)
│   │         Transition → Idle (Foreman decides what happens next)
│   │
│   └── STATE: SelfMaintenance               ← minor self-repair during idle time
│         Condition: degraded subsystem AND idle AND repair SmartObject available nearby
│         Tasks:
│           1. Claim maintenance SmartObject (self-directed — Foreman aware but not mediating)
│           2. Navigate + interact (wipe lens, recalibrate sensor, minor patch)
│         Duration: short (5-15s game time)
│         Transition → Idle when done
│         Transition → Commanded if Foreman interrupts with priority task
│
└── ★ STATE: OffDuty                        ← shift ended
    │
    │   Worker is autonomous. Foreman has no authority.
    │   Same structure as Foreman's OffDuty — shared linked StateTree.
    │
    ├── STATE: PersonalMaintenance           ← self-repair, recharge
    │
    ├── STATE: Recreation
    │   │
    │   │   Androids aren't sentient but they ARE aware. Recreation isn't
    │   │   "fun" — it's operational downtime with emergent behavioral
    │   │   patterns. Think: machine learning cool-down cycles that happen
    │   │   to look like a robot sitting on a rock staring at the sky.
    │   │
    │   ├── AutonomousWander                 ← explore within safe perimeter
    │   ├── ObserveEnvironment               ← stop, track moving objects, scan horizon
    │   ├── SocialClustering                 ← drift toward other idle androids
    │   │                                       NOT conversation — just spatial proximity
    │   │                                       Pattern: androids end up in loose groups
    │   │                                       without anyone deciding to group up
    │   ├── InspectNovelObject               ← approach unfamiliar actors, examine briefly
    │   └── ReturnToRestPoint                ← claim a bench/alcove SmartObject, enter low-power idle
    │
    └── STATE: EmergencyRecall               ← base alert → snap to OnDuty
```

### 3.2 — The Self-Preservation Exception

Your architecture doc states: *"Workers do not autonomously find SmartObjects — the Foreman is always the coordinator."*

This design preserves that rule with **two explicit exceptions**, both justified in-fiction:

1. **EmergencySelfPreserve** — Power critical, about to die. The android's hardcoded survival firmware overrides the command hierarchy. It finds its own charging station. The Foreman is notified but does not mediate. This is a hardware-level override, not a choice.

2. **CommLinkLost → AutonomousWork** — Can't receive commands. The android falls back to basic autonomous operation. When comm is restored, it immediately re-registers with the Foreman and returns to mediated operation.

Both exceptions make the Foreman-mediated pattern *stronger* because they define the boundary conditions. The hierarchy works until physics or communication makes it impossible.

### 3.3 — SelfMaintenance vs Repair Bay

Two different things:

| | SelfMaintenance | Repair Bay |
|---|---|---|
| **Who decides** | Worker (autonomous, during idle) | Foreman (Triage state) or Worker (EmergencySelfPreserve) |
| **What it fixes** | Minor degradation: muddy lens, slight misalignment | Major damage: structural, destroyed subsystems, limb replacement |
| **SmartObject** | `SOD_SelfMaintenance` — small, everywhere, quick | `SOD_Repair` — repair bay, limited, long interaction |
| **Capability tag removed?** | No — subsystem is Degraded, not Destroyed | Yes — this restores Destroyed subsystems |
| **Foreman involved?** | Aware but not commanding | Commands worker to repair bay via Triage |
| **Duration** | 5–15 seconds | 30–120+ seconds |

---

## Part 4 — Shared Autonomous Behavior (Linked StateTree)

Both Foreman and Workers share the same off-duty behavior. Rather than duplicating, use a **Linked StateTree** asset: `ST_AutonomousBehavior`.

### 4.1 — ST_AutonomousBehavior (Linked)

```
ST_AutonomousBehavior (Linked StateTree — embedded in OffDuty states)
│
├── [Evaluator] AndroidConditionEval
│
├── STATE: MaintFirst                       ← always check body state before recreation
│     Condition: any subsystem Degraded OR PowerLevel < 0.7
│     Task: FindNearestMaintenanceSmartObject
│     Task: Navigate + Interact (recharge / self-repair)
│     Transition → Recreate when: all clear
│
└── STATE: Recreate
    │
    ├── [Selector: weighted random, biased by "personality seed"]
    │
    ├── STATE: Wander                        weight: 30%
    │     Task: PickRandomNavPoint(radius 800–1500)
    │     Task: NavigateTo
    │     Task: PauseAndLookAround(3–8s)
    │     Loop 2–4 times → re-select
    │
    ├── STATE: Observe                       weight: 25%
    │     Task: FindElevatedPoint OR FindOpenSightline
    │     Task: NavigateTo
    │     Task: ScanHorizon(rotate head slowly, 10–20s)
    │     Transition → re-select
    │
    ├── STATE: Cluster                       weight: 20%
    │     Task: FindNearestIdleAndroid(radius 2000)
    │     Task: NavigateToProximity(not ON them — 150–300 units away)
    │     Task: MatchOrientation (face same general direction)
    │     Task: IdleInPlace(15–30s)
    │     NOTE: Two androids both doing Cluster creates emergent grouping
    │           without any explicit "form group" logic
    │
    ├── STATE: Inspect                       weight: 15%
    │     Task: FindNearestNovelActor(not previously inspected within 300s)
    │     Task: NavigateTo(approach distance 200)
    │     Task: OrientToward + HeadTrack(5–10s)
    │     Task: MarkAsInspected(actor, timestamp)
    │     Transition → re-select
    │
    └── STATE: Rest                          weight: 10%
          Task: FindAndClaimSmartObject(Task.Idle)  ← bench, alcove, wall lean-point
          Task: NavigateToSmartObject
          Task: EnterLowPowerIdle(60–180s)
          Transition → re-select
```

**Personality Seed:** Each android gets a random uint32 at fabrication, stored on the ConditionComponent. It biases the weighted random selection. Some androids wander more. Some cluster more. Some rest more. No two groups of androids produce identical crowd behavior. This is cosmetic — not gameplay affecting — but sells the illusion of individuality hard.

### 4.2 — What Makes This Feel Alive

None of these behaviors are complex individually. The emergent quality comes from:

- **Cluster + Cluster** → two androids independently decide to approach each other → they end up standing near each other facing the same direction → looks like friends hanging out
- **Observe + high ground** → android climbs a hill and stares at the gas giant → looks contemplative
- **Inspect + novel object** → android walks over to a newly placed building, looks at it, walks away → looks curious
- **Rest + bench SmartObject** → android sits on a bench in low-power mode → looks tired
- **Wander + pause** → android walks, stops, looks around, walks again → looks like someone thinking

The player sees *personality*. The system sees weighted random with a seed.

---

## Part 5 — Foreman Triage Logic (New StateTree Task)

### 5.1 — FForemanTask_Triage (C++)

```
USTRUCT()
struct FForemanTask_Triage : public FGameplayStateTreeTaskBase

  // Called when WorkAvailabilityEval reports bHasDamagedWorkers == true
  
  EStateTreeRunStatus EnterState():
    DamagedWorkers = GetAllWorkers().Filter(w => w.Readiness < WorkReady)
    Sort by severity (Disabled first, then NeedsRepair, then Degraded)
    
    for each worker:
      if worker.PowerState == Dead:
        // Can't help — flag for manual recovery, remove from active roster
        MarkForRecovery(worker)
        continue
        
      if worker.PowerState == Critical:
        // Already in EmergencySelfPreserve — just acknowledge
        continue
        
      if worker has active task:
        // Reassign their task to next-best worker
        ReclaimSmartObjectSlot(worker.CurrentTask)
        QueueForReassignment(worker.CurrentTask)
        
      if worker.Readiness == NeedsRepair:
        // Send to repair bay
        RepairSlot = QuerySmartObject(Task.Repair, unclaimed)
        if RepairSlot.IsValid():
          SendCommand(worker, ClaimAndInteract(RepairSlot))
        else:
          // No repair bay available — worker waits in degraded idle
          AddToRepairQueue(worker)
          
      if worker.CommLink == Lost:
        // Can't command them. Note and move on.
        // Worker's own CommLinkLost state handles autonomous behavior
        FlagCommLost(worker)

    return EStateTreeRunStatus::Succeeded  // always returns to Dispatch
```

### 5.2 — Worker Damage Report Flow

Workers don't wait for the Foreman to notice damage. They actively report:

```
// On UAndroidConditionComponent — called when any subsystem changes status
void OnSubsystemChanged(ESubsystemType System, ESubsystemStatus OldStatus, ESubsystemStatus NewStatus)
{
    RecalculateCapabilities();
    
    // Report to Foreman if comm link is active
    if (CommLink != ESubsystemStatus::Destroyed)
    {
        if (AForeman_AIController* Foreman = FindForeman())
        {
            Foreman->OnWorkerConditionChanged(GetOwner(), GetReadiness(), ActiveCapabilities);
        }
    }
    // If comm is lost, Foreman won't know until it does a manual check
    // or the worker re-establishes connection
}
```

The Foreman's `OnWorkerConditionChanged` sets `bHasDamagedWorkers = true` on the evaluator, which triggers the Triage transition.

---

## Part 6 — Shift System (On-Duty / Off-Duty)

### 6.1 — Simple Shift Clock

Not a real-time clock. A game-time cycle driven by a world subsystem or the Foreman itself.

```
UCLASS()
class UShiftManager : public UWorldSubsystem

  float ShiftDurationSeconds;     // Game-time length of a work shift
  float BreakDurationSeconds;     // Game-time length of off-duty period
  EShiftPhase CurrentPhase;       // Working, Break
  float PhaseTimeRemaining;
  
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShiftChanged, EShiftPhase, NewPhase);
  FOnShiftChanged OnShiftChanged;
```

Both Foreman and Workers bind to `OnShiftChanged`. When the shift ends:

- **Foreman:** Completes current dispatch cycle, transitions to OffDuty. Does NOT abandon workers mid-task — waits for active assignments to complete or reach a checkpoint, then releases authority.
- **Workers:** Active workers finish their current task, then transition to OffDuty instead of returning to Idle. Idle workers transition immediately.

When the shift starts:
- **Foreman:** Transitions to OnDuty. Re-evaluates all worker readiness. Enters Dispatch.
- **Workers:** Transition to OnDuty → Idle, awaiting commands.

### 6.2 — Graceful Shift Transition

The shift boundary is soft, not hard. A worker carrying a block doesn't drop it when the whistle blows. The StateTree handles this naturally:

```
// In ST_Worker, the OnDuty → OffDuty transition condition:
Condition: IsOnDuty == false AND CurrentState != Commanded
// Worker must finish current command before transitioning
// If in Idle or SelfMaintenance, transitions immediately
```

---

## Part 7 — Specific Failure Scenarios

These show how the system handles interesting situations with NO special-case code — just the general architecture above working together.

### 7.1 — Vision Mud Buildup

```
Trigger:     Environment event (dust storm, splash, proximity to excavation)
Effect:      VisionStatus → Degraded
Consequence: AIPerception sight radius halved (multiplier on ConditionComponent)
Worker:      If idle → SelfMaintenance state → finds SOD_SelfMaintenance → "wipe lens" interaction
             If working → continues at reduced perception, reports degraded to Foreman
Foreman:     Receives condition report. May reassign to less vision-critical task at next cycle.
Recovery:    SelfMaintenance interaction restores VisionStatus → Operational
Visual:      Animation of android wiping hand across face/visor
```

### 7.2 — Limb Loss (Combat or Accident)

```
Trigger:     Damage event exceeding limb HP threshold
Effect:      ManipulatorLeft/Right → Destroyed, ActiveCapabilities recalculated
Consequence: Capability.Hauling removed (if both arms gone) or reduced carry capacity
Worker:      Reports to Foreman. Readiness → NeedsRepair.
Foreman:     Triage state → sends worker to repair bay (SOD_Repair)
             Reassigns worker's current task to next available worker
Recovery:    Repair bay IWytchWorkSite interaction → restores limb → RecalculateCapabilities
Visual:      Worker walks with arm stump. Repair bay animation shows arm reattachment.
Duration:    Long repair — worker is offline for a real gameplay-significant period
```

### 7.3 — Comm Link Lost

```
Trigger:     EM storm, distance from relay, combat damage to comm array
Effect:      CommLink → Lost
Consequence: Worker cannot receive Foreman commands. Foreman cannot query worker state.
Worker:      ST_Worker transitions to CommLinkLost state
             → tries to return to base (last known position)
             → if navigation fails, wanders in limited radius
             → periodically retries comm (every 30s)
             → MAY autonomously work visible SmartObjects (advanced behavior)
Foreman:     Does NOT know worker is lost until next Monitor tick fails to get response
             → flags worker as CommLost
             → does NOT reassign to that worker
             → if task was in progress, reclaims and reassigns
Recovery:    Comm restored (storm passes, distance decreases, repair)
             → worker immediately re-registers with Foreman
             → reports current condition
             → transitions back to OnDuty → Idle
Visual:      Antenna/head-light flickers. Worker moves uncertainly. Pauses frequently.
Gameplay:    Player might see a lone android wandering far from base during a storm.
             That's a lost worker. The player or another android can approach to "help"
             (bring within comm range, escort back).
```

### 7.4 — Foreman Goes Down

```
Trigger:     Foreman reaches Disabled state (power dead + structural failure)
Consequence: No Foreman. Workers in OnDuty_Idle stay idle. No new dispatches.
             Workers mid-task finish their current task, then idle indefinitely.
             Workers in EmergencySelfPreserve are unaffected (hardcoded).
Recovery:    Player must repair Foreman. Or — future system — a replacement Foreman
             is designated from workers with highest intelligence profile.
Visual:      Workers standing around. Idle fidgets. Gradually drifting into SelfMaintenance
             and OffDuty-like behavior as time passes without commands. Settlement feels
             like it's winding down. Eerie.
Gameplay:    Urgency driver — get the Foreman back online or everything stalls.
```

---

## Part 8 — New C++ Classes Summary

| Class | Type | Lives On | Purpose |
|---|---|---|---|
| `UAndroidConditionComponent` | UActorComponent | Every android Character | Power, structural HP, subsystem status, dynamic capabilities |
| `UShiftManager` | UWorldSubsystem | World | Shift clock, broadcasts phase changes |
| `FForemanTask_Triage` | StateTree Task | ST_Foreman | Evaluate damaged workers, reassign tasks, send to repair |
| `FForemanCondition_HasDamagedWorkers` | StateTree Condition | ST_Foreman | Checks evaluator output for damaged worker flag |
| `FAndroidConditionEval` | StateTree Evaluator | ST_Foreman & ST_Worker | Reads ConditionComponent, outputs state to StateTree context |
| `FWorkerTask_SelfMaintenance` | StateTree Task | ST_Worker | Self-directed minor repair using SOD_SelfMaintenance |
| `FWorkerTask_ReportCondition` | StateTree Task | ST_Worker | Notify Foreman of condition changes |
| `FAutonomousTask_Wander` | StateTree Task | ST_AutonomousBehavior | Random navigation with pauses |
| `FAutonomousTask_Cluster` | StateTree Task | ST_AutonomousBehavior | Move near other idle androids |
| `FAutonomousTask_Observe` | StateTree Task | ST_AutonomousBehavior | Find sightline, scan environment |
| `FAutonomousTask_Inspect` | StateTree Task | ST_AutonomousBehavior | Approach and examine novel actors |

### SmartObject Definitions (New Assets)

| Asset | Tag | Purpose |
|---|---|---|
| `SOD_Repair` | `Task.Repair` | Repair bay — restores destroyed subsystems, structural HP |
| `SOD_Recharge` | `Task.Recharge` | Charging station — restores power |
| `SOD_SelfMaintenance` | `Task.SelfMaintenance` | Quick-access maintenance point — clears Degraded status |
| `SOD_IdleRest` | `Task.Idle` | Bench, alcove, wall lean — off-duty rest points |

All implement `IWytchWorkSite`. No new interfaces needed.

---

## Part 9 — Implementation Order

This builds on your existing roadmap without disrupting it. Start after SmartObject foundation (roadmap steps 1–3) is validated.

| Phase | What | Depends On | Structural Risk |
|---|---|---|---|
| **A** | `UAndroidConditionComponent` — power, HP, subsystem enums, `RecalculateCapabilities()` | Nothing — pure data component | None |
| **B** | `FAndroidConditionEval` — StateTree evaluator reading component | Phase A | None — additive evaluator |
| **C** | Dynamic capability tags — workers return `ActiveCapabilities` from component instead of static tags | Phase A + existing capability system (roadmap step 4) | Low — one getter change |
| **D** | Worker StateTree restructure — add Disabled, EmergencySelfPreserve, CommLinkLost root states around existing OnDuty logic | Phase B + existing worker StateTree | Medium — StateTree restructure |
| **E** | Foreman StateTree restructure — add Disabled, EmergencySelfPreserve, OffDuty, Triage states | Phase B + existing ST_Foreman | Medium — StateTree restructure |
| **F** | `UShiftManager` + shift transitions | Phase D + E | Low — event-driven, additive |
| **G** | `ST_AutonomousBehavior` linked StateTree — wander, observe, cluster, inspect, rest | Phase F | Low — fully new, no refactoring |
| **H** | `SOD_Repair`, `SOD_Recharge`, `SOD_SelfMaintenance`, `SOD_IdleRest` SmartObject definitions + corresponding IWytchWorkSite implementations | Roadmap steps 1–3 (SmartObject foundation) | Low — same pattern as SOD_Build |
| **I** | Damage events — hook into UE damage system to feed ConditionComponent | Phase A | Low — standard UE pattern |

**Phases A–C can begin now**, in parallel with your SmartObject roadmap. They're pure data with no dependencies on the SmartObject work.

---

## Part 10 — Performance Notes

- `UAndroidConditionComponent` ticks on a **1–2 second timer**, not per-frame. Power drain and condition checks don't need 60Hz.
- `RecalculateCapabilities()` only runs **on subsystem change events**, not on tick. Tag container operations are fast but there's no reason to do them speculatively.
- `FAndroidConditionEval` runs at **0.5s intervals** (matching existing `FForemanEval_WorkAvailability`). StateTree evaluators are efficient at this cadence.
- Autonomous behavior tasks use **NavMesh queries** (already available) and **SmartObject subsystem queries** (already in architecture). No new spatial query systems.
- `SocialClustering` uses a simple **nearest-neighbor query** on the worker roster — O(n) where n is worker count. Fast up to hundreds.
- **Personality seed** is a single uint32 read. Weighted random selection is trivially cheap.
- All condition evaluation, state transitions, and capability math is **pure C++**. Zero Blueprint overhead in the AI tick path.
