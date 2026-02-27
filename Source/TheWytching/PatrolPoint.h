#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PatrolPoint.generated.h"

class UBillboardComponent;

UCLASS(Blueprintable)
class THEWYTCHING_API APatrolPoint : public AActor
{
	GENERATED_BODY()

public:
	APatrolPoint();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol")
	FString Label;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBillboardComponent> Billboard;
};
