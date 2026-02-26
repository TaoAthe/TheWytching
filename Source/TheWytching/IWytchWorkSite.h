#pragma once

#include "CoreMinimal.h"
#include "IWytchInteractable.h"
#include "IWytchWorkSite.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UWytchWorkSite : public UWytchInteractable
{
	GENERATED_BODY()
};

/**
 * Anywhere work can be performed by a worker.
 * Extends IWytchInteractable — GetInteractionType() returns "Work" by default.
 */
class THEWYTCHING_API IWytchWorkSite : public IWytchInteractable
{
	GENERATED_BODY()

public:
	/** Claim this site for a worker. Returns true if claim succeeded. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	bool TryClaim(AActor* Worker);

	/** Release this site (worker finished or abandoned). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	void ReleaseSite(AActor* Worker);

	/** Who currently has this site claimed? nullptr if unclaimed. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	AActor* GetClaimant() const;

	/** What type of work is performed here? (e.g. "Build", "Harvest") */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wytch|WorkSite")
	FString GetWorkType() const;
};
