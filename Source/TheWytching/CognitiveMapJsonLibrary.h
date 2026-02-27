#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CognitiveMapJsonLibrary.generated.h"

UCLASS()
class THEWYTCHING_API UCognitiveMapJsonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Cognitive|Json")
	static bool SaveCognitiveMapToJsonFile(
		const TMap<FString, FString>& EntriesMap,
		FString& OutFilePath);

	UFUNCTION(BlueprintCallable, Category = "Cognitive|Json")
	static bool BuildSensorReadoutFromJsonFile(
		FString& OutReadout,
		FString& OutFilePath,
		int32& OutCount);
};

