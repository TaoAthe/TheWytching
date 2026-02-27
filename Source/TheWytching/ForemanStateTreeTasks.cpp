#include "ForemanStateTreeTasks.h"

#include "ForemanTypes.h"
#include "Foreman_AIController.h"
#include "Foreman_BrainComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ForemanSurveyComponent.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"

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
	if (!Pawn)
	{
		UE_LOG(LogForeman, Warning, TEXT("PlanJob: No pawn — failing"));
		return EStateTreeRunStatus::Failed;
	}

	PlayMontageOnPawn(Pawn, Montage);
	// For now, scan by class name — workstations register with Foreman at BeginPlay
	// and are tracked in BP_FOREMAN_V2's RegisteredWorkStations array.
	// This C++ task will find them via world query.

	const FVector ForemanLocation = Pawn->GetActorLocation();
	float BestScore = TNumericLimits<float>::Max();
	AActor* BestTarget = nullptr;
	AActor* BestWorker = nullptr;

	// Find available workstations (actors with tag "WorkStation")
	TArray<AActor*> WorkStations;
	UGameplayStatics::GetAllActorsWithTag(
		Pawn->GetWorld(), FName(TEXT("WorkStation")), WorkStations);

	for (AActor* WS : WorkStations)
	{
		if (!WS) continue;

		// Check availability via Blueprint-callable function
		// For C++ interop, we use a simple tag/property check.
		// TODO: Replace with interface call once BPI_WorkTarget is C++
		bool bAvailable = !WS->Tags.Contains(FName(TEXT("Claimed")));
		if (!bAvailable) continue;

		float Distance = FVector::Dist(ForemanLocation, WS->GetActorLocation());
		if (Distance < BestScore)
		{
			BestScore = Distance;
			BestTarget = WS;
		}
	}

	// Find idle workers (actors with tag "Worker" and state check)
	TArray<AActor*> Workers;
	UGameplayStatics::GetAllActorsWithTag(
		Pawn->GetWorld(), FName(TEXT("Worker")), Workers);

	for (AActor* Worker : Workers)
	{
		if (!Worker) continue;

		// TODO: Query GetWorkerState() via interface once BPI_WorkerCommand is C++
		// For now, check a simple "Idle" tag
		if (Worker->Tags.Contains(FName(TEXT("Idle"))))
		{
			BestWorker = Worker;
			break;
		}
	}

	Data.SelectedWorkTarget = BestTarget;
	Data.SelectedWorker = BestWorker;

	if (BestTarget && BestWorker)
	{
		UE_LOG(LogForeman, Log, TEXT("PlanJob: Selected target=%s worker=%s distance=%.0f"),
			*BestTarget->GetName(), *BestWorker->GetName(), BestScore);
		return EStateTreeRunStatus::Succeeded;
	}

	UE_LOG(LogForeman, Log, TEXT("PlanJob: No valid target/worker pair (targets=%d workers=%d)"),
		WorkStations.Num(), Workers.Num());
	return EStateTreeRunStatus::Failed;
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
	if (!Pawn)
	{
		UE_LOG(LogForeman, Warning, TEXT("AssignWorker: No pawn — failing"));
		return EStateTreeRunStatus::Failed;
	}

	PlayMontageOnPawn(Pawn, Montage);

	// AssignWorker delegates to the Blueprint dispatch logic for now.
	// The BP_FOREMAN_V2 Blueprint has the validated AssignWork function
	// that handles TryClaimTask + ReceiveCommand.
	//
	// TODO: Port AssignWork logic to C++ once BPI_WorkerCommand and
	//       BPI_WorkTarget are migrated to C++ interfaces.

	UE_LOG(LogForeman, Log, TEXT("AssignWorker: Delegating to Blueprint dispatch loop"));
	return EStateTreeRunStatus::Succeeded;
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
			if (Brain)
			{
				Brain->IssueCommand(TEXT("scan_environment"));
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
