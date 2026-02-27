#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Perception/AIPerceptionComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Foreman_BrainComponent.generated.h"

UENUM()
enum class EForemanState : uint8
{
	Idle,
	LookingAround,
	NavigatingToTarget,
	PickingUp,
	NavigatingToDestination,
	Placing,
	TaskComplete
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THEWYTCHING_API UForeman_BrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForeman_BrainComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// Called externally to give Kellan a task
	void IssueCommand(const FString& Command);

private:
	// State
	EForemanState CurrentState;
	FString CurrentCommand;
	FString TargetActorTag;
	FVector TargetLocation;

	UPROPERTY()
	AActor* HeldActor;

	UPROPERTY()
	TArray<AActor*> LastPerceivedActors;

	// Perception tracking
	UPROPERTY()
	AActor* PerceivedRedCone = nullptr;

	// Components
	UPROPERTY()
	USceneCaptureComponent2D* SceneCapture;

	UPROPERTY()
	UAIPerceptionComponent* PerceptionComponent;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget;

	// Look around state
	float LookAroundAngle;
	float LookAroundTimer;
	int32 LookAroundSnapsCount;
	bool bWaitingForLLMResponse;
	float InitialForwardYaw;
	bool bSavedUseControllerDesiredRotation;
	bool bSavedOrientRotationToMovement;
	FRotator SavedRotationRate;
	bool bHasSavedRotationSettings;

	// Settings
	UPROPERTY(EditAnywhere, Category="Foreman")
	float SnapInterval;

	UPROPERTY(EditAnywhere, Category="Foreman")
	FString LMStudioURL;

	// Vision
	void InitialiseComponents();
	void SnapAndAnalyse();
	FString RenderTargetToBase64();
	FString BuildPerceptionContext();
	void SendToLLM(const FString& Base64, const FString& Context);
	void OnLLMResponse(FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful);
	FString SanitizeJson(const FString& Raw);

	// Action execution
	void HandleMoveToComplete();
	void PickUpActor(AActor* Target);
	void PlaceHeldActor(FVector Location);

	// State transitions
	void SetState(EForemanState NewState);
	void TickLookAround(float DeltaTime);
	void TickNavigating(float DeltaTime);

	// Perception
	UFUNCTION()
	void OnTargetPerceptionUpdated(const FActorPerceptionUpdateInfo& UpdateInfo);

	// Helpers
	AActor* FindActorByTag(const FString& Tag);
	AActor* GetForemanActor() const;
	APawn* GetForemanPawn() const;
	class AForeman_AIController* GetForemanController();
};

