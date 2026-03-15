#include "ForemanStateTreeConditions.h"
#include "ForemanTypes.h"

#include "Foreman_AIController.h"
#include "IWytchCommandable.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "GameFramework/Pawn.h"

// ─────────────────────────────────────────────────────────
// FForemanCondition_HasIdleWorkers
// ─────────────────────────────────────────────────────────

bool FForemanCondition_HasIdleWorkers::TestCondition(
	FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	APawn* Pawn = Data.Pawn.Get();
	if (!Pawn)
	{
		UE_LOG(LogForeman, Warning, TEXT("HasIdleWorkers: no Pawn — returning false"));
		return false;
	}

	AForeman_AIController* ForemanAIC = Cast<AForeman_AIController>(Pawn->GetController());
	if (!ForemanAIC)
	{
		UE_LOG(LogForeman, Warning, TEXT("HasIdleWorkers: no AForeman_AIController — returning false"));
		return false;
	}

	int32 RosterSize = 0;
	for (const TWeakObjectPtr<AActor>& WeakWorker : ForemanAIC->GetRegisteredWorkers())
	{
		AActor* Worker = WeakWorker.Get();
		if (!Worker) continue;
		++RosterSize;
		if (IWytchCommandable* Commandable = Cast<IWytchCommandable>(Worker))
		{
			const EWorkerState State = Commandable->Execute_GetWorkerState(Worker);
			UE_LOG(LogForeman, Log, TEXT("HasIdleWorkers: [%s] state=%d"),
				*Worker->GetName(), (int32)State);
			if (State == EWorkerState::Idle)
			{
				UE_LOG(LogForeman, Log, TEXT("HasIdleWorkers: TRUE (found idle worker: %s)"), *Worker->GetName());
				return true;
			}
		}
	}

	UE_LOG(LogForeman, Log, TEXT("HasIdleWorkers: FALSE (roster=%d, none idle)"), RosterSize);
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
		UE_LOG(LogForeman, Warning, TEXT("HasAvailableWork: no Pawn — returning false"));
		return false;
	}

	USmartObjectSubsystem* SOSubsystem = USmartObjectSubsystem::GetCurrent(Pawn->GetWorld());
	if (!SOSubsystem)
	{
		UE_LOG(LogForeman, Warning, TEXT("HasAvailableWork: no SmartObjectSubsystem — returning false"));
		return false;
	}

	FSmartObjectRequest Request;
	Request.QueryBox = FBox::BuildAABB(Pawn->GetActorLocation(), FVector(5000.f));

	TArray<FSmartObjectRequestResult> Results;
	SOSubsystem->FindSmartObjects(Request, Results);

	UE_LOG(LogForeman, Log, TEXT("HasAvailableWork: found %d SmartObject result(s)"), Results.Num());

	for (const FSmartObjectRequestResult& Result : Results)
	{
		const ESmartObjectSlotState SlotState = SOSubsystem->GetSlotState(Result.SlotHandle);
		UE_LOG(LogForeman, Log, TEXT("HasAvailableWork: slot state=%d (0=Free,1=Claimed,2=Disabled)"),
			(int32)SlotState);
		if (SlotState == ESmartObjectSlotState::Free)
		{
			UE_LOG(LogForeman, Log, TEXT("HasAvailableWork: TRUE (free slot found)"));
			return true;
		}
	}

	UE_LOG(LogForeman, Log, TEXT("HasAvailableWork: FALSE (no free slots)"));
	return false;
}
