#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SmartObjectRuntime.h"
#include "AndroidTypes.h"
#include "IWytchCommandable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UWytchCommandable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Command interface for Foreman-to-worker communication.
 * Workers implement this; the Foreman casts to IWytchCommandable to assign tasks,
 * query state, and abort work.
 *
 * Separate from IWytch* interfaces:
 *   - IWytchInteractable/Carryable/WorkSite = what OBJECTS do when interacted with
 *   - IWytchCommandable = what WORKERS do when commanded by the Foreman
 */
class THEWYTCHING_API IWytchCommandable
{
	GENERATED_BODY()

public:
	/** Foreman calls this to assign a SmartObject-based task to the worker. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Command")
	void ReceiveSmartObjectAssignment(
		FSmartObjectClaimHandle ClaimHandle,
		FSmartObjectSlotHandle SlotHandle,
		AActor* TargetActor);

	/** Foreman calls this to query the worker's current state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Command")
	EWorkerState GetWorkerState() const;

	/** Foreman calls this to get the worker's capability tags. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Command")
	FGameplayTagContainer GetCapabilities() const;

	/** Foreman calls this to abort the worker's current task. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Command")
	void AbortCurrentTask(EAbortReason Reason);
};
