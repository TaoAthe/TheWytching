#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "OllamaDronePawn.generated.h"

UCLASS()
class THEWYTCHING_API AOllamaDronePawn : public APawn
{
	GENERATED_BODY()

public:
	AOllamaDronePawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(
		UInputComponent* PlayerInputComponent) override;

private:
	UPROPERTY(VisibleAnywhere)
	UBoxComponent* BoxCollision;

	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* SceneCapture;

	// TopDownCapture - commented out temporarily
	// UPROPERTY(VisibleAnywhere)
	// USceneCaptureComponent2D* TopDownCapture;

	UPROPERTY(VisibleAnywhere)
	class UCameraComponent* DroneCamera;

	UPROPERTY(VisibleAnywhere)
	class UAIPerceptionComponent* PerceptionComponent;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget;

	// TopDownRenderTarget - commented out temporarily
	// UPROPERTY()
	// UTextureRenderTarget2D* TopDownRenderTarget;

	// Movement
	void MoveForward(float Value);
	void MoveRight(float Value);
	void MoveUp(float Value);
	void Turn(float Value);
	void LookUp(float Value);

	// Perception
	UPROPERTY(EditAnywhere, Category="Drone|Perception")
	float SightRadius = 2000.f;

	UPROPERTY(EditAnywhere, Category="Drone|Perception")
	float LoseSightRadius = 2500.f;

	UPROPERTY(EditAnywhere, Category="Drone|Perception")
	float PeripheralVisionAngle = 60.f;

	FString BuildPerceptionContext();

	// Vision
	void SnapAndSend();
	FString RenderTargetToBase64(UTextureRenderTarget2D* Target);
	void SendImageToLLM(const FString& Base64Image,
	                    const FString& ContextText);
	void OnResponseReceived(FHttpRequestPtr Request,
	                       FHttpResponsePtr Response,
	                       bool bWasSuccessful);

	FString BuildContextText() const;
	float TraceDistance(const FVector& Direction,
	                    float MaxDistance) const;

	// Movement input cache
	FVector MovementInput;
	float TurnInput;
	float LookInput;

	// Settings
	UPROPERTY(EditAnywhere, Category="Drone")
	float MoveSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category="Drone")
	float LookSensitivity = 1.5f;

	UPROPERTY(EditAnywhere, Category="Drone|Vision")
	bool bIncludeTopDownImage = true;

	UPROPERTY(EditAnywhere, Category="Drone|Vision")
	float TopDownHeight = 800.f;

	UPROPERTY(EditAnywhere, Category="Drone|Vision")
	float TopDownOrthoWidth = 2000.f;

	UPROPERTY(EditAnywhere, Category="Drone|Vision")
	int32 CaptureSize = 512;

	UPROPERTY(EditAnywhere, Category="Drone|Vision")
	float TraceMaxDistance = 5000.f;

	UPROPERTY(EditAnywhere, Category="Drone|Vision")
	float NearbyScanRadius = 2000.f;

	UPROPERTY(EditAnywhere, Category="Drone|Vision")
	int32 MaxNearbyActors = 12;

	// Debug
	void DebugInput() const;
};

