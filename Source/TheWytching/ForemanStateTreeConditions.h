#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeExecutionContext.h"
#include "ForemanStateTreeConditions.generated.h"

class APawn;

// ─────────────────────────────────────────────────────────
// Shared condition instance data
// ─────────────────────────────────────────────────────────
USTRUCT()
struct FForemanConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<APawn> Pawn = nullptr;
};

// ─────────────────────────────────────────────────────────
// FForemanCondition_HasIdleWorkers
//   True when at least one registered worker is in Idle state.
// ─────────────────────────────────────────────────────────
USTRUCT(meta = (DisplayName = "Foreman: Has Idle Workers"))
struct THEWYTCHING_API FForemanCondition_HasIdleWorkers : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// ─────────────────────────────────────────────────────────
// FForemanCondition_HasAvailableWork
//   True when at least one workstation is available (unclaimed).
// ─────────────────────────────────────────────────────────
USTRUCT(meta = (DisplayName = "Foreman: Has Available Work"))
struct THEWYTCHING_API FForemanCondition_HasAvailableWork : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
