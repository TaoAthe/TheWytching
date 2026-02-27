// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OllamaDebugSpawner.generated.h"

/**
 * Simple spawner that creates an OllamaDebugActor on BeginPlay.
 * Place this in your level instead of manually placing OllamaDebugActor.
 */
UCLASS()
class THEWYTCHING_API AOllamaDebugSpawner : public AActor
{
	GENERATED_BODY()
	
public:
	AOllamaDebugSpawner();
	
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
	TSubclassOf<AActor> OllamaDebugActorClass;
};

