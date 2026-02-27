#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AndroidTypes.h"
#include "AndroidConditionComponent.generated.h"

/**
 * Tracks an android's physical condition: power, structural integrity,
 * subsystem health, and derived capability state. Foundation of the
 * autonomy + degradation system.
 *
 * Tick runs on a 1-2 second timer for power drain.
 * RecalculateCapabilities() fires only on subsystem change events.
 */
UCLASS(ClassGroup=(Wytcherly), meta=(BlueprintSpawnableComponent))
class THEWYTCHING_API UAndroidConditionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAndroidConditionComponent();

	// ── Power ──

	/** Current power level, 0.0 = dead, 1.0 = full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Power", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PowerLevel = 1.0f;

	/** Power drain per second. Varies by activity — set externally by StateTree tasks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Power", meta = (ClampMin = "0.0"))
	float PowerDrainRate = 0.001f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Android|Power")
	EAndroidPowerState PowerState = EAndroidPowerState::Normal;

	// ── Structural ──

	/** Structural hit points, 0.0 = destroyed, 1.0 = pristine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Structural", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StructuralHP = 1.0f;

	// ── Subsystems ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Subsystems")
	ESubsystemStatus VisionStatus = ESubsystemStatus::Operational;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Subsystems")
	ESubsystemStatus AudioStatus = ESubsystemStatus::Operational;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Subsystems")
	ESubsystemStatus LocomotionStatus = ESubsystemStatus::Operational;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Subsystems")
	ESubsystemStatus ManipulatorLeft = ESubsystemStatus::Operational;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Subsystems")
	ESubsystemStatus ManipulatorRight = ESubsystemStatus::Operational;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Subsystems")
	ESubsystemStatus CommLink = ESubsystemStatus::Operational;

	// ── Capabilities ──

	/** Base capabilities set at fabrication. Does not change at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Capabilities")
	FGameplayTagContainer BaseCapabilities;

	/** Active capabilities, recalculated when subsystems change. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Android|Capabilities")
	FGameplayTagContainer ActiveCapabilities;

	// ── Personality ──

	/** Random seed assigned at fabrication. Biases autonomous behavior choices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Android|Personality")
	int32 PersonalitySeed = 0;

	// ── Delegates ──

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSubsystemChanged,
		ESubsystemType, System,
		ESubsystemStatus, OldStatus,
		ESubsystemStatus, NewStatus);

	UPROPERTY(BlueprintAssignable, Category = "Android|Events")
	FOnSubsystemChanged OnSubsystemChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPowerStateChanged,
		EAndroidPowerState, OldState,
		EAndroidPowerState, NewState);

	UPROPERTY(BlueprintAssignable, Category = "Android|Events")
	FOnPowerStateChanged OnPowerStateChanged;

	// ── Interface ──

	/** Recalculates ActiveCapabilities based on current subsystem health. */
	UFUNCTION(BlueprintCallable, Category = "Android|Condition")
	void RecalculateCapabilities();

	/** Returns overall readiness assessment. */
	UFUNCTION(BlueprintCallable, Category = "Android|Condition")
	EAndroidReadiness GetReadiness() const;

	/** Sets a subsystem's status and fires OnSubsystemChanged + RecalculateCapabilities. */
	UFUNCTION(BlueprintCallable, Category = "Android|Condition")
	void SetSubsystemStatus(ESubsystemType System, ESubsystemStatus NewStatus);

	/** Gets the status of a specific subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Android|Condition")
	ESubsystemStatus GetSubsystemStatus(ESubsystemType System) const;

protected:
	virtual void BeginPlay() override;

private:
	/** Timer-driven power drain. */
	void DrainPower();

	/** Updates PowerState from PowerLevel and broadcasts if changed. */
	void UpdatePowerState();

	/** Returns a reference to the subsystem status field for a given type. */
	ESubsystemStatus& GetSubsystemStatusRef(ESubsystemType System);

	FTimerHandle PowerDrainTimerHandle;

	/** How often power drain ticks (seconds). */
	UPROPERTY(EditAnywhere, Category = "Android|Power", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float PowerDrainInterval = 1.5f;
};
