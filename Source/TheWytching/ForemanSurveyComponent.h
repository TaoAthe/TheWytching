#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForemanSurveyComponent.generated.h"

class APatrolPoint;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THEWYTCHING_API UForemanSurveyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForemanSurveyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survey")
	TArray<TObjectPtr<APatrolPoint>> PatrolPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survey")
	float SurveyRadius = 500.0f;

	UFUNCTION(BlueprintCallable, Category = "Survey")
	APatrolPoint* GetNextPatrolPoint();

	UFUNCTION(BlueprintCallable, Category = "Survey")
	int32 CountNearbyWorkers(FVector Origin);

	UFUNCTION(BlueprintCallable, Category = "Survey")
	FString GenerateStatusReport();

private:
	int32 CurrentPatrolIndex = 0;
};
