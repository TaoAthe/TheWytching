#pragma once

#include "CoreMinimal.h"
#include "IWytchInteractable.h"
#include "IWytchCarryable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UWytchCarryable : public UWytchInteractable
{
	GENERATED_BODY()
};

/**
 * Anything that can be picked up and moved by a worker.
 * Extends IWytchInteractable — GetInteractionType() returns "Carry" by default.
 */
class THEWYTCHING_API IWytchCarryable : public IWytchInteractable
{
	GENERATED_BODY()

public:
	/** Attach this object to the carrying worker. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Carry")
	void OnPickedUp(AActor* Carrier);

	/** Detach this object from the carrier at the given location. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Carry")
	void OnPutDown(FVector DropLocation);

	/** Where should this object be delivered? */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Carry")
	FVector GetDeliveryLocation() const;

	/** Has this object reached its destination? */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|Carry")
	bool IsAtDestination() const;
};
