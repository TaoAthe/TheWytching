// Copyright Epic Games, Inc. All Rights Reserved.

#include "OllamaDebugActor.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Base64.h"
#include "ImageUtils.h"

AOllamaDebugActor::AOllamaDebugActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(
		TEXT("SceneCapture"));
	RootComponent = SceneCapture;
}

void AOllamaDebugActor::BeginPlay()
{
	Super::BeginPlay();

	// Create render target at runtime
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->InitAutoFormat(512, 512);
	RenderTarget->UpdateResourceImmediate(true);

	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->CaptureSource = SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	// Slight delay to let level fully load before capturing
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this,
		&AOllamaDebugActor::CaptureAndSend, 1.0f, false);
}

void AOllamaDebugActor::CaptureAndSend()
{
	// Trigger the capture
	SceneCapture->CaptureScene();

	FString Base64 = RenderTargetToBase64();

	if (Base64.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Ollama: Failed to encode render target"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Ollama: Image captured, sending to LMStudio..."));
	SendImageToLLM(Base64);
}

FString AOllamaDebugActor::RenderTargetToBase64()
{
	if (!RenderTarget) return FString();

	FRenderTarget* RT = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RT) return FString();

	TArray<FColor> Pixels;
	if (!RT->ReadPixels(Pixels)) return FString();

	// Get dimensions
	int32 Width = RenderTarget->SizeX;
	int32 Height = RenderTarget->SizeY;

	// Convert to PNG via ImageWrapper
	IImageWrapperModule& ImageWrapperModule = 
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(
			FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = 
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	ImageWrapper->SetRaw(Pixels.GetData(), 
		Pixels.GetAllocatedSize(), Width, Height, 
		ERGBFormat::BGRA, 8);

	TArray64<uint8> PNGData = ImageWrapper->GetCompressed(0);

	// Convert to Base64
	return FBase64::Encode(PNGData.GetData(), PNGData.Num());
}

void AOllamaDebugActor::SendImageToLLM(const FString& Base64Image)
{
	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();

	Request->SetURL(TEXT("http://localhost:1234/v1/chat/completions"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Build JSON with image
	FString Body = FString::Printf(TEXT(R"({
		"model": "liquid/lfm2.5-vl-1.6b",
		"messages": [
			{
				"role": "user",
				"content": [
					{
						"type": "image_url",
						"image_url": {
							"url": "data:image/png;base64,%s"
						}
					},
					{
						"type": "text",
						"text": "Provide a comprehensive analysis of this game scene. List ALL visible objects, characters, and environmental elements. For each item, describe: 1) What it is, 2) Its color/appearance, 3) Its approximate position (left/right/center, near/far), 4) Any notable features. Be thorough and detailed."
					}
				]
			}
		],
		"temperature": 0.3,
		"stream": false
	})"), *Base64Image);

	Request->SetContentAsString(Body);
	Request->OnProcessRequestComplete().BindUObject(this,
		&AOllamaDebugActor::OnResponseReceived);
	Request->ProcessRequest();
}

void AOllamaDebugActor::OnResponseReceived(FHttpRequestPtr Request,
                                            FHttpResponsePtr Response,
                                            bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Ollama: Request failed"));
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
			TEXT("Ollama: Failed to connect"));
		return;
	}

	// Parse the OpenAI wrapper to get content
	FString Raw = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("Ollama Raw Response: %s"), *Raw);
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);

	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		TArray<TSharedPtr<FJsonValue>> Choices = 
			JsonObject->GetArrayField(TEXT("choices"));
		if (Choices.Num() > 0)
		{
			FString Content = Choices[0]->AsObject()
				->GetObjectField(TEXT("message"))
				->GetStringField(TEXT("content"));

			// Log the full description with clear formatting
			UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════════════════════"));
			UE_LOG(LogTemp, Warning, TEXT("║ LLM SCENE ANALYSIS:"));
			UE_LOG(LogTemp, Warning, TEXT("╠═══════════════════════════════════════════════════════════"));
			
			// Split by newlines for better logging
			TArray<FString> Lines;
			Content.ParseIntoArray(Lines, TEXT("\n"), true);
			
			for (const FString& Line : Lines)
			{
				if (!Line.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("║ %s"), *Line.TrimStartAndEnd());
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════════════════════"));

			// Try to parse as structured JSON first (backward compatible)
			TSharedPtr<FJsonObject> ContentJson;
			TSharedRef<TJsonReader<>> ContentReader = TJsonReaderFactory<>::Create(Content);
			
			if (FJsonSerializer::Deserialize(ContentReader, ContentJson) && 
			    ContentJson->HasField(TEXT("action")))
			{
				// Structured response with action/target/reasoning
				FString Action = ContentJson->GetStringField(TEXT("action"));
				FString Target = ContentJson->GetStringField(TEXT("target"));
				FString Reasoning = ContentJson->HasField(TEXT("reasoning")) ? 
					ContentJson->GetStringField(TEXT("reasoning")) : TEXT("");

				GEngine->AddOnScreenDebugMessage(-1, 25.f, FColor::Green,
					FString::Printf(TEXT("🎯 Action: %s"), *Action));
				GEngine->AddOnScreenDebugMessage(-1, 25.f, FColor::Yellow,
					FString::Printf(TEXT("🎪 Target: %s"), *Target));
				GEngine->AddOnScreenDebugMessage(-1, 25.f, FColor::Cyan,
					FString::Printf(TEXT("🧠 Reasoning: %s"), *Reasoning));
			}
			else
			{
				// Comprehensive text description - display with line breaks
				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::White,
					TEXT("═══════ LLM VISION ANALYSIS ═══════"));
				
				int32 MessageKey = 100;  // Start with a high key to avoid conflicts
				for (const FString& Line : Lines)
				{
					if (!Line.IsEmpty())
					{
						// Alternate colors for readability
						FColor LineColor = (MessageKey % 2 == 0) ? FColor::Cyan : FColor::White;
						GEngine->AddOnScreenDebugMessage(MessageKey++, 30.f, LineColor,
							Line.TrimStartAndEnd());
					}
				}
				
				GEngine->AddOnScreenDebugMessage(MessageKey++, 2.f, FColor::White,
					TEXT("═══════════════════════════════════"));
			}
		}
	}
}

