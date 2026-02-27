#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionContext.h"
#include "ForemanStateTreeEvaluators.generated.h"

class APawn;

// ─────────────────────────────────────────────────────────
// FForemanEval_WorkAvailability
//   Ticks continuously. Maintains cached counts of idle
//   workers and available workstations. Exposed as output
//   properties for conditions and tasks to bind against.
// ─────────────────────────────────────────────────────────
USTRUCT()
struct FForemanEval_WorkAvailabilityInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<APawn> Pawn = nullptr;

	// Output: live counts updated every tick
	UPROPERTY(EditAnywhere, Category = "Output")
	int32 IdleWorkerCount = 0;

	UPROPERTY(EditAnywhere, Category = "Output")
	int32 AvailableWorkCount = 0;

	UPROPERTY(EditAnywhere, Category = "Output")
	int32 ActiveAssignmentCount = 0;

	// Internal tick throttle
	float TimeSinceLastScan = 0.f;
};

USTRUCT(meta = (DisplayName = "Foreman: Work Availability"))
struct THEWYTCHING_API FForemanEval_WorkAvailability : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FForemanEval_WorkAvailabilityInstanceData;

	// How often to rescan (seconds). Lower = more responsive, higher = cheaper.
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ScanInterval = 0.5f;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context,
		const float DeltaTime) const override;

private:
	void ScanWorld(FInstanceDataType& Data) const;
};
