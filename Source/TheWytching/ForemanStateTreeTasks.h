#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimMontage.h"
#include "ForemanStateTreeTasks.generated.h"

class AAIController;
class APawn;
class UForeman_BrainComponent;

// ─────────────────────────────────────────────────────────
// Shared instance data: all Foreman tasks need the controller + pawn
// ─────────────────────────────────────────────────────────
USTRUCT()
struct FForemanTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller = nullptr;

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<APawn> Pawn = nullptr;
};

// ─────────────────────────────────────────────────────────
// FForemanTask_PlanJob
//   Scans registered workstations, scores by distance/priority,
//   selects the best available target for dispatch.
// ─────────────────────────────────────────────────────────
USTRUCT()
struct FForemanTask_PlanJobInstanceData : public FForemanTaskInstanceData
{
	GENERATED_BODY()

	// Internal: the selected work target from scanning
	UPROPERTY()
	TObjectPtr<AActor> SelectedWorkTarget = nullptr;

	// Internal: the selected idle worker
	UPROPERTY()
	TObjectPtr<AActor> SelectedWorker = nullptr;
};

USTRUCT(meta = (DisplayName = "Foreman: Plan Job"))
struct THEWYTCHING_API FForemanTask_PlanJob : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanTask_PlanJobInstanceData;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context,
		const float DeltaTime) const override;
};

// ─────────────────────────────────────────────────────────
// FForemanTask_AssignWorker
//   Claims the selected workstation and sends the Build
//   command to the paired worker.
// ─────────────────────────────────────────────────────────
USTRUCT(meta = (DisplayName = "Foreman: Assign Worker"))
struct THEWYTCHING_API FForemanTask_AssignWorker : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanTaskInstanceData;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

// ─────────────────────────────────────────────────────────
// FForemanTask_Monitor
//   Foreman moves to an oversight position and monitors
//   active worker progress. Succeeds when no active work.
// ─────────────────────────────────────────────────────────
USTRUCT()
struct FForemanTask_MonitorInstanceData : public FForemanTaskInstanceData
{
	GENERATED_BODY()

	float ElapsedTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Foreman: Monitor"))
struct THEWYTCHING_API FForemanTask_Monitor : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanTask_MonitorInstanceData;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float CheckInterval = 3.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context,
		const float DeltaTime) const override;

	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

// ─────────────────────────────────────────────────────────
// FForemanTask_Rally
//   Calls all registered workers to the Foreman's position.
// ─────────────────────────────────────────────────────────
USTRUCT(meta = (DisplayName = "Foreman: Rally Workers"))
struct THEWYTCHING_API FForemanTask_Rally : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanTaskInstanceData;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

// ─────────────────────────────────────────────────────────
// FForemanTask_Wait
//   Idle state — waits for a configurable duration then succeeds.
//   Optionally triggers a BrainComponent LLM scan.
// ─────────────────────────────────────────────────────────
USTRUCT()
struct FForemanTask_WaitInstanceData : public FForemanTaskInstanceData
{
	GENERATED_BODY()

	float ElapsedTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Foreman: Wait"))
struct THEWYTCHING_API FForemanTask_Wait : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanTask_WaitInstanceData;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Duration = 5.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bTriggerLLMScan = false;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context,
		const float DeltaTime) const override;

	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
