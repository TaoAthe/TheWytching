#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Foreman_AIController.generated.h"

class UStateTree;
class UStateTreeAIComponent;
class UAISenseConfig_Sight;
class UForeman_BrainComponent;

UCLASS()
class THEWYTCHING_API AForeman_AIController : public AAIController
{
	GENERATED_BODY()

public:
	AForeman_AIController();
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Movement helpers
	void ExecuteMoveToLocation(FVector Destination);
	void ExecuteMoveToActor(AActor* Target);

	// Accessors
	UFUNCTION(BlueprintCallable, Category = "Foreman")
	UForeman_BrainComponent* GetForemanBrain() const { return ForemanBrain; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Foreman|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foreman|AI")
	TObjectPtr<UStateTree> DefaultStateTree;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Foreman|Brain")
	TObjectPtr<UForeman_BrainComponent> ForemanBrain;

private:
	void ConfigurePerception();
};
