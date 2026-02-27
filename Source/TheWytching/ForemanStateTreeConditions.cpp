#include "ForemanStateTreeConditions.h"

#include "ForemanTypes.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// ─────────────────────────────────────────────────────────
// FForemanCondition_HasIdleWorkers
// ─────────────────────────────────────────────────────────

bool FForemanCondition_HasIdleWorkers::TestCondition(
	FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	APawn* Pawn = Data.Pawn.Get();
	if (!Pawn || !Pawn->GetWorld())
	{
		return false;
	}

	TArray<AActor*> Workers;
	UGameplayStatics::GetAllActorsWithTag(
		Pawn->GetWorld(), FName(TEXT("Worker")), Workers);

	for (AActor* Worker : Workers)
	{
		if (Worker && Worker->Tags.Contains(FName(TEXT("Idle"))))
		{
			return true;
		}
	}

	// TODO: Replace tag-based check with BPI_WorkerCommand::GetWorkerState()
	//       once the interface is migrated to C++.
	return false;
}

// ─────────────────────────────────────────────────────────
// FForemanCondition_HasAvailableWork
// ─────────────────────────────────────────────────────────

bool FForemanCondition_HasAvailableWork::TestCondition(
	FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	APawn* Pawn = Data.Pawn.Get();
	if (!Pawn || !Pawn->GetWorld())
	{
		return false;
	}

	TArray<AActor*> WorkStations;
	UGameplayStatics::GetAllActorsWithTag(
		Pawn->GetWorld(), FName(TEXT("WorkStation")), WorkStations);

	for (AActor* WS : WorkStations)
	{
		if (WS && !WS->Tags.Contains(FName(TEXT("Claimed"))))
		{
			return true;
		}
	}

	// TODO: Replace tag-based check with BPI_WorkTarget::IsTaskAvailable()
	//       once the interface is migrated to C++.
	return false;
}
