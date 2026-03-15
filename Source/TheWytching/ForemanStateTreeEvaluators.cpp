#include "ForemanStateTreeEvaluators.h"

#include "ForemanTypes.h"
#include "Foreman_AIController.h"
#include "IWytchCommandable.h"
#include "AndroidConditionComponent.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "GameFramework/Pawn.h"

// ─────────────────────────────────────────────────────────
// FForemanEval_WorkAvailability
// ─────────────────────────────────────────────────────────

void FForemanEval_WorkAvailability::TreeStart(
	FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	// Do NOT scan immediately on TreeStart — workers register via BeginPlay which fires
	// on the same frame as (or just after) the StateTree starts. A scan here would always
	// return 0 idle workers. Start the interval counter at 0 so the first real scan fires
	// after one full ScanInterval, by which time all workers will be registered.
	Data.TimeSinceLastScan = 0.f;
	Data.IdleWorkerCount = 0;
	Data.AvailableWorkCount = 0;
	Data.ActiveAssignmentCount = 0;

	UE_LOG(LogForeman, Log,
		TEXT("WorkAvailability evaluator started — first scan in %.1fs"),
		ScanInterval);
}

void FForemanEval_WorkAvailability::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	Data.TimeSinceLastScan += DeltaTime;

	if (Data.TimeSinceLastScan >= ScanInterval)
	{
		Data.TimeSinceLastScan = 0.f;
		ScanWorld(Data);
	}
}

void FForemanEval_WorkAvailability::ScanWorld(FInstanceDataType& Data) const
{
	APawn* Pawn = Data.Pawn.Get();
	if (!Pawn || !Pawn->GetWorld())
	{
		Data.IdleWorkerCount = 0;
		Data.AvailableWorkCount = 0;
		Data.ActiveAssignmentCount = 0;
		return;
	}

	UWorld* World = Pawn->GetWorld();

	// ── 1. Idle worker count via roster + IWytchCommandable::GetWorkerState() ──

	AForeman_AIController* ForemanAIC = Cast<AForeman_AIController>(Pawn->GetController());
	int32 IdleCount = 0;
	int32 ActiveCount = 0;

	if (ForemanAIC)
	{
		const TArray<TWeakObjectPtr<AActor>>& Roster = ForemanAIC->GetRegisteredWorkers();
		for (const TWeakObjectPtr<AActor>& WeakWorker : Roster)
		{
			AActor* Worker = WeakWorker.Get();
			if (!Worker) continue;

			if (IWytchCommandable* Commandable = Cast<IWytchCommandable>(Worker))
			{
				const EWorkerState State = Commandable->Execute_GetWorkerState(Worker);
				if (State == EWorkerState::Idle)
				{
					++IdleCount;
				}
				else if (State == EWorkerState::Working || State == EWorkerState::MovingToTask)
				{
					++ActiveCount;
				}
			}
		}
	}
	else
	{
		UE_LOG(LogForeman, Warning,
			TEXT("WorkAvailabilityEval::ScanWorld — Pawn has no AForeman_AIController, idle count will be 0"));
	}

	// ── 2. Available work count via SmartObject subsystem ──

	int32 AvailableCount = 0;

	if (USmartObjectSubsystem* SOSubsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		// GetCurrent() returns null if the subsystem isn't ready — no further guard needed.
		FSmartObjectRequest Request;
		// Query sphere centred on the Foreman — 5000 units covers the work zone
		Request.QueryBox = FBox::BuildAABB(Pawn->GetActorLocation(), FVector(5000.f));

		TArray<FSmartObjectRequestResult> Results;
		SOSubsystem->FindSmartObjects(Request, Results);

		for (const FSmartObjectRequestResult& Result : Results)
		{
			// Only count unclaimed slots — claimed slots belong to active assignments
			// (already counted above via worker state)
			const ESmartObjectSlotState SlotState =
				SOSubsystem->GetSlotState(Result.SlotHandle);

			if (SlotState == ESmartObjectSlotState::Free)
			{
				++AvailableCount;
			}
		}
	}

	Data.IdleWorkerCount = IdleCount;
	Data.AvailableWorkCount = AvailableCount;
	Data.ActiveAssignmentCount = ActiveCount;

	UE_LOG(LogForeman, Log,
		TEXT("WorkAvailability scan: idle=%d available=%d active=%d"),
		IdleCount, AvailableCount, ActiveCount);
}
