// Copyright Epic Games, Inc. All Rights Reserved.

#include "OllamaDebugSpawner.h"
#include "OllamaDebugActor.h"

AOllamaDebugSpawner::AOllamaDebugSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// Set default class to OllamaDebugActor
	OllamaDebugActorClass = AOllamaDebugActor::StaticClass();
}

void AOllamaDebugSpawner::BeginPlay()
{
	Super::BeginPlay();
	
	if (OllamaDebugActorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		
		AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(OllamaDebugActorClass, GetActorLocation(), GetActorRotation(), SpawnParams);
		
		if (SpawnedActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("OllamaDebugSpawner: Successfully spawned %s"), *OllamaDebugActorClass->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OllamaDebugSpawner: OllamaDebugActorClass is not set!"));
	}
}

