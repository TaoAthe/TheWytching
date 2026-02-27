#include "ForemanStateTreeEvaluators.h"

#include "ForemanTypes.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// ─────────────────────────────────────────────────────────
// FForemanEval_WorkAvailability
// ─────────────────────────────────────────────────────────

void FForemanEval_WorkAvailability::TreeStart(
	FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	Data.TimeSinceLastScan = ScanInterval; // Force immediate scan on start
	ScanWorld(Data);

	UE_LOG(LogForeman, Log,
		TEXT("WorkAvailability evaluator started: %d idle workers, %d available work, %d active"),
		Data.IdleWorkerCount, Data.AvailableWorkCount, Data.ActiveAssignmentCount);
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

	// Count idle workers
	TArray<AActor*> Workers;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("Worker")), Workers);

	int32 IdleCount = 0;
	for (AActor* Worker : Workers)
	{
		if (Worker && Worker->Tags.Contains(FName(TEXT("Idle"))))
		{
			IdleCount++;
		}
	}

	// Count available / claimed workstations
	TArray<AActor*> WorkStations;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("WorkStation")), WorkStations);

	int32 AvailableCount = 0;
	int32 ActiveCount = 0;
	for (AActor* WS : WorkStations)
	{
		if (!WS) continue;
		if (WS->Tags.Contains(FName(TEXT("Claimed"))))
		{
			ActiveCount++;
		}
		else
		{
			AvailableCount++;
		}
	}

	Data.IdleWorkerCount = IdleCount;
	Data.AvailableWorkCount = AvailableCount;
	Data.ActiveAssignmentCount = ActiveCount;

	// TODO: Replace tag-based scanning with C++ interface queries
	//       (BPI_WorkerCommand::GetWorkerState, BPI_WorkTarget::IsTaskAvailable)
	//       once interfaces are migrated to C++.
}
