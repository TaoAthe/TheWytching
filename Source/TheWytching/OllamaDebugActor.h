// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "OllamaDebugActor.generated.h"

UCLASS()
class THEWYTCHING_API AOllamaDebugActor : public AActor
{
	GENERATED_BODY()
	
public:
	AOllamaDebugActor();
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* SceneCapture;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget;

	void CaptureAndSend();
	FString RenderTargetToBase64();
	void SendImageToLLM(const FString& Base64Image);
	void OnResponseReceived(FHttpRequestPtr Request,
	                       FHttpResponsePtr Response,
	                       bool bWasSuccessful);
};

