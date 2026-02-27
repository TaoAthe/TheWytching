#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "IWytchInteractable.h"
#include "AndroidTypes.h"
#include "IWytchWorkSite.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UWytchWorkSite : public UWytchInteractable
{
	GENERATED_BODY()
};

/**
 * Anywhere work can be performed by a worker.
 * Extends IWytchInteractable — GetInteractionType() returns "Work" by default.
 *
 * Claiming is handled by SmartObjects (USmartObjectSubsystem).
 * This interface defines WHAT HAPPENS during the work interaction:
 *   SmartObject gets the worker there → BeginWork → TickWork → EndWork
 */
class THEWYTCHING_API IWytchWorkSite : public IWytchInteractable
{
	GENERATED_BODY()

public:
	/** What task type is this site? Returns a GameplayTag (e.g. Task.Build, Task.Repair). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	FGameplayTag GetTaskType() const;

	/** How long does interaction take in seconds? 0 = instant. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	float GetInteractionDuration() const;

	/** Begin the work interaction. Returns true if work can begin. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	bool BeginWork(AActor* Worker);

	/** Called each tick during work (progress bars, animations, etc.). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	void TickWork(AActor* Worker, float DeltaTime);

	/** End interaction. Called on completion, abort, or failure. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	void EndWork(AActor* Worker, EWorkEndReason Reason);

	/** Is this site currently functional? (could be destroyed, unpowered, etc.) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	bool IsOperational() const;
};
