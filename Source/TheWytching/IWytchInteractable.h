#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IWytchInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UWytchInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Base interface for anything in the world that can be interacted with.
 * Workers receive generic "go to X, interact" commands — the object defines what "interact" means.
 *
 * Hierarchy:
 *   IWytchInteractable        — base (this)
 *     IWytchCarryable          — can be picked up and moved
 *     IWytchWorkSite           — where work can be performed
 */
class THEWYTCHING_API IWytchInteractable
{
	GENERATED_BODY()

public:
	/** What kind of interaction this object offers (e.g. "Carry", "Build", "Harvest"). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Interaction")
	FString GetInteractionType() const;

	/** Can this object be interacted with right now? */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Interaction")
	bool IsInteractionAvailable() const;

	/** Where the worker should stand to interact with this object. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Interaction")
	FVector GetInteractionLocation() const;
};
