#include "ForemanStateTreeTasks.h"

#include "ForemanTypes.h"
#include "Foreman_AIController.h"
#include "Foreman_BrainComponent.h"
#include "IWytchCommandable.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ForemanSurveyComponent.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectRequestTypes.h"

namespace
{
	void PlayMontageOnPawn(APawn* Pawn, UAnimMontage* Montage)
	{
		if (!Montage || !Pawn) return;
		if (USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance())
			{
				Anim->Montage_Play(Montage);
			}
		}
	}

	void StopMontageOnPawn(APawn* Pawn, UAnimMontage* Montage)
	{
		if (!Montage || !Pawn) return;
		if (USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance())
			{
				Anim->Montage_Stop(0.25f, Montage);
			}
		}
	}
}

// ─────────────────────────────────────────────────────────
// FForemanTask_PlanJob
// ─────────────────────────────────────────────────────────

EStateTreeRunStatus FForemanTask_PlanJob::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	APawn* Pawn = Data.Pawn.Get();
	if (!Pawn || !Pawn->GetWorld())
	{
		UE_LOG(LogForeman, Warning, TEXT("PlanJob: No pawn — failing"));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Pawn->GetWorld();

	PlayMontageOnPawn(Pawn, Montage);

	// ── 1. Find available SmartObject slots ──
	// GetCurrent() returns null if the subsystem isn't ready — no further guard needed.
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

	// Pick the closest Free slot
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


EStateTreeRunStatus FForemanTask_PlanJob::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	// PlanJob completes instantly in EnterState — this shouldn't fire
	return EStateTreeRunStatus::Succeeded;
}

// ─────────────────────────────────────────────────────────
// FForemanTask_AssignWorker
// ─────────────────────────────────────────────────────────

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
		// Pass invalid ClaimHandle — worker claims on arrival (DEC-004)
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

// ─────────────────────────────────────────────────────────
// FForemanTask_Monitor
// ─────────────────────────────────────────────────────────

EStateTreeRunStatus FForemanTask_Monitor::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	Data.ElapsedTime = 0.f;

	PlayMontageOnPawn(Data.Pawn.Get(), Montage);

	UE_LOG(LogForeman, Log, TEXT("Monitor: Entering oversight mode (interval=%.1fs)"),
		CheckInterval);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FForemanTask_Monitor::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	Data.ElapsedTime += DeltaTime;

	if (Data.ElapsedTime < CheckInterval)
	{
		return EStateTreeRunStatus::Running;
	}

	Data.ElapsedTime = 0.f;

	// Check if any workers are still actively working
	APawn* Pawn = Data.Pawn.Get();
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	UForemanSurveyComponent* Survey = Pawn->FindComponentByClass<UForemanSurveyComponent>();
	if (Survey)
	{
		FString Report = Survey->GenerateStatusReport();
		UE_LOG(LogForeman, Warning, TEXT("%s"), *Report);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, Report);
	}

	TArray<AActor*> WorkStations;
	UGameplayStatics::GetAllActorsWithTag(
		Pawn->GetWorld(), FName(TEXT("WorkStation")), WorkStations);

	bool bAnyActive = false;
	for (AActor* WS : WorkStations)
	{
		if (WS && WS->Tags.Contains(FName(TEXT("Claimed"))))
		{
			bAnyActive = true;
			break;
		}
	}

	if (!bAnyActive)
	{
		UE_LOG(LogForeman, Log, TEXT("Monitor: No active work — returning to dispatch"));
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

// ─────────────────────────────────────────────────────────
// FForemanTask_Rally
// ─────────────────────────────────────────────────────────

EStateTreeRunStatus FForemanTask_Rally::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	APawn* Pawn = Data.Pawn.Get();
	if (!Pawn)
	{
		UE_LOG(LogForeman, Warning, TEXT("Rally: No pawn — failing"));
		return EStateTreeRunStatus::Failed;
	}

	PlayMontageOnPawn(Pawn, Montage);

	const FVector RallyPoint = Pawn->GetActorLocation();

	TArray<AActor*> Workers;
	UGameplayStatics::GetAllActorsWithTag(
		Pawn->GetWorld(), FName(TEXT("Worker")), Workers);

	int32 Count = 0;
	for (AActor* Worker : Workers)
	{
		if (!Worker) continue;

		// Send rally command via interface message
		// TODO: Use C++ interface once BPI_WorkerCommand is migrated
		UE_LOG(LogForeman, Log, TEXT("Rally: Calling %s to %.0f,%.0f,%.0f"),
			*Worker->GetName(), RallyPoint.X, RallyPoint.Y, RallyPoint.Z);
		Count++;
	}

	UE_LOG(LogForeman, Log, TEXT("Rally: Called %d workers"), Count);
	return EStateTreeRunStatus::Succeeded;
}

// ─────────────────────────────────────────────────────────
// FForemanTask_Wait
// ─────────────────────────────────────────────────────────

EStateTreeRunStatus FForemanTask_Wait::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	Data.ElapsedTime = 0.f;

	PlayMontageOnPawn(Data.Pawn.Get(), Montage);

	UE_LOG(LogForeman, Log, TEXT("Wait: Idling for %.1fs (LLM scan=%s)"),
		Duration, bTriggerLLMScan ? TEXT("yes") : TEXT("no"));

	if (bTriggerLLMScan)
	{
		AAIController* AIC = Data.Controller.Get();
		AForeman_AIController* ForemanAIC = Cast<AForeman_AIController>(AIC);
		if (ForemanAIC)
		{
			UForeman_BrainComponent* Brain = ForemanAIC->GetForemanBrain();
			if (Brain && Brain->IsBooted())
			{
				Brain->IssueCommand(TEXT("scan_environment"));
			}
			else
			{
				UE_LOG(LogForeman, Verbose,
					TEXT("Wait: skipping LLM scan, ForemanBrain not booted yet"));
			}
		}
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FForemanTask_Wait::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	Data.ElapsedTime += DeltaTime;

	if (Data.ElapsedTime >= Duration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}


// ─────────────────────────────────────────────────────────
// Monitor / Wait — ExitState (stop montage on state exit)
// ─────────────────────────────────────────────────────────

void FForemanTask_Monitor::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	StopMontageOnPawn(Data.Pawn.Get(), Montage);
}

void FForemanTask_Wait::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	StopMontageOnPawn(Data.Pawn.Get(), Montage);
}
