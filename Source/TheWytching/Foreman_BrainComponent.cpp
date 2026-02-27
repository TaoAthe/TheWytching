#include "Foreman_BrainComponent.h"
#include "Foreman_AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "Json.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
	FString EscapeForJson(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Out.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Out.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Out.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return Out;
	}
}

UForeman_BrainComponent::UForeman_BrainComponent()
	: CurrentState(EForemanState::Idle)
	, CurrentCommand()
	, TargetActorTag()
	, TargetLocation(FVector::ZeroVector)
	, HeldActor(nullptr)
	, LastPerceivedActors()
	, SceneCapture(nullptr)
	, PerceptionComponent(nullptr)
	, RenderTarget(nullptr)
	, LookAroundAngle(0.f)
	, LookAroundTimer(0.f)
	, LookAroundSnapsCount(0)
	, bWaitingForLLMResponse(false)
	, InitialForwardYaw(0.f)
	, bSavedUseControllerDesiredRotation(false)
	, bSavedOrientRotationToMovement(false)
	, SavedRotationRate(FRotator::ZeroRotator)
	, bHasSavedRotationSettings(false)
	, SnapInterval(2.f)
	, LMStudioURL(TEXT("http://localhost:1234/v1/chat/completions"))
{
	// Tick drives the perception + state machine loop.
	PrimaryComponentTick.bCanEverTick = true;
}

void UForeman_BrainComponent::BeginPlay()
{
	Super::BeginPlay();
	InitialiseComponents();
}

void UForeman_BrainComponent::InitialiseComponents()
{
	AActor* Owner = GetOwner();
	AActor* ForemanActor = GetForemanActor();
	if (!Owner || !ForemanActor)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Foreman: Brain initialise failed (Owner=%s ForemanActor=%s)"),
			Owner ? *Owner->GetName() : TEXT("None"),
			ForemanActor ? *ForemanActor->GetName() : TEXT("None"));
		return;
	}

	// Create render target
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->InitAutoFormat(256, 256);
	RenderTarget->UpdateResourceImmediate(true);

	// Find or create SceneCapture on head socket
	SceneCapture = NewObject<USceneCaptureComponent2D>(ForemanActor,
		TEXT("ForemanEye"));
	SceneCapture->RegisterComponent();

	// Attach to head socket if skeletal mesh exists
	USkeletalMeshComponent* Mesh =
		ForemanActor->FindComponentByClass<USkeletalMeshComponent>();
	if (Mesh && Mesh->DoesSocketExist(TEXT("head")))
	{
		SceneCapture->AttachToComponent(Mesh,
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			TEXT("head"));
	}
	else
	{
		USceneComponent* RootComponent = ForemanActor->GetRootComponent();
		if (!RootComponent)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Foreman: No root component for %s"),
				*ForemanActor->GetName());
			return;
		}

		SceneCapture->AttachToComponent(RootComponent,
			FAttachmentTransformRules::SnapToTargetIncludingScale);
		SceneCapture->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	}

	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->CaptureSource = SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	// AIPerception: prefer controller ownership when available.
	AActor* PerceptionOwner = Owner;
	if (AForeman_AIController* Controller = GetForemanController())
	{
		PerceptionOwner = Controller;
	}

	PerceptionComponent =
		PerceptionOwner->FindComponentByClass<UAIPerceptionComponent>();
	if (!PerceptionComponent)
	{
		PerceptionComponent = NewObject<UAIPerceptionComponent>(PerceptionOwner,
			TEXT("ForemanPerception"));
		PerceptionComponent->RegisterComponent();
	}
	UAIPerceptionSystem::RegisterPerceptionStimuliSource(
		GetWorld(), UAISense_Sight::StaticClass(), GetOwner());

	UAISenseConfig_Sight* SightConfig =
		NewObject<UAISenseConfig_Sight>(PerceptionComponent);
	SightConfig->SightRadius = 2000.f;
	SightConfig->LoseSightRadius = 2500.f;
	SightConfig->PeripheralVisionAngleDegrees = 60.f;
	SightConfig->SetMaxAge(5.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(
		SightConfig->GetSenseImplementation());
	PerceptionComponent->RequestStimuliListenerUpdate();

	// Bind to the info-updated delegate that matches our handler signature.
	PerceptionComponent->OnTargetPerceptionInfoUpdated.AddDynamic(
		this, &UForeman_BrainComponent::OnTargetPerceptionUpdated);

	UE_LOG(LogTemp, Warning,
		TEXT("Foreman: Kellan brain initialised, ready for commands"));
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
		TEXT("Foreman Kellan: Ready"));
}

void UForeman_BrainComponent::IssueCommand(const FString& Command)
{
	UE_LOG(LogTemp, Warning,
		TEXT("Foreman: IssueCommand called: %s"), *Command);
	CurrentCommand = Command;
	UE_LOG(LogTemp, Warning, TEXT("Foreman: Command received - %s"),
		*Command);
	GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Yellow,
		FString::Printf(TEXT("Foreman Command: %s"), *Command));

	// Save current forward yaw
	AActor* ForemanActor = GetForemanActor();
	if (ForemanActor)
	{
		InitialForwardYaw = ForemanActor->GetActorRotation().Yaw;
		UE_LOG(LogTemp, Warning, TEXT("Foreman: Saved initial yaw: %.1f"), InitialForwardYaw);
		GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Cyan,
			FString::Printf(TEXT("Initial Yaw: %.1f"), InitialForwardYaw));
	}

	SetState(EForemanState::LookingAround);
	UE_LOG(LogTemp, Warning,
		TEXT("Foreman: State set to LookingAround"));
}

void UForeman_BrainComponent::SetState(EForemanState NewState)
{
	const EForemanState PreviousState = CurrentState;
	CurrentState = NewState;
	LookAroundTimer = 0.f;
	LookAroundAngle = 0.f;
	LookAroundSnapsCount = 0;
	bWaitingForLLMResponse = false;

	if (NewState == EForemanState::LookingAround)
	{
		if (AForeman_AIController* Controller = GetForemanController())
		{
			Controller->StopMovement();
			Controller->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}

	APawn* ForemanPawn = GetForemanPawn();
	ACharacter* ForemanCharacter = Cast<ACharacter>(ForemanPawn);
	UCharacterMovementComponent* MoveComp = ForemanCharacter
		? ForemanCharacter->GetCharacterMovement()
		: nullptr;

	if (NewState == EForemanState::LookingAround && MoveComp)
	{
		if (!bHasSavedRotationSettings)
		{
			bSavedUseControllerDesiredRotation = MoveComp->bUseControllerDesiredRotation;
			bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
			SavedRotationRate = MoveComp->RotationRate;
			bHasSavedRotationSettings = true;
		}

		// Force body yaw to follow controller yaw while scanning.
		MoveComp->bUseControllerDesiredRotation = true;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
	}
	else if (PreviousState == EForemanState::LookingAround &&
		NewState != EForemanState::LookingAround &&
		MoveComp && bHasSavedRotationSettings)
	{
		MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		MoveComp->RotationRate = SavedRotationRate;
		bHasSavedRotationSettings = false;
	}

	UE_LOG(LogTemp, Warning, TEXT("Foreman: State -> %d"),
		(int32)NewState);
}

void UForeman_BrainComponent::TickComponent(float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurrentState)
	{
		case EForemanState::LookingAround:
			TickLookAround(DeltaTime);
			break;
		case EForemanState::NavigatingToTarget:
		case EForemanState::NavigatingToDestination:
			TickNavigating(DeltaTime);
			break;
		default:
			break;
	}
}

void UForeman_BrainComponent::TickLookAround(float DeltaTime)
{
	if (bWaitingForLLMResponse) return;

	LookAroundTimer += DeltaTime;
	if (LookAroundTimer < SnapInterval) return;
	LookAroundTimer = 0.f;

	// Rotate Foreman to scan the scene
	AActor* ForemanActor = GetForemanActor();
	if (ForemanActor && LookAroundSnapsCount < 4)
	{
		float StartYaw = InitialForwardYaw;
		float CurrentYaw = StartYaw + (LookAroundSnapsCount * 90.f);
		FRotator ScanRotation(0.f, FRotator::NormalizeAxis(CurrentYaw), 0.f);

		UE_LOG(LogTemp, Warning, 
			TEXT("Foreman: Snap %d rotating to yaw %.1f"),
			LookAroundSnapsCount, CurrentYaw);

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
			FString::Printf(TEXT("Snap %d → Yaw: %.1f (start: %.1f + offset: %d*90)"),
				LookAroundSnapsCount, CurrentYaw, StartYaw, LookAroundSnapsCount));

		// CRITICAL: Rotate the entire actor body (not just controller/head)
		// SetControlRotation only moves the head - we need the whole capsule to turn
		// so the SceneCapture component faces the right direction
		ForemanActor->SetActorRotation(ScanRotation);

		// Also sync controller for movement consistency
		if (AForeman_AIController* Controller = GetForemanController())
		{
			Controller->SetControlRotation(ScanRotation);
		}
	}

	LookAroundSnapsCount++;
	
	// IMPORTANT: Delay capture until next frame so rotation fully updates
	// Queue the capture with a small delay to let the visual update
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UForeman_BrainComponent::SnapAndAnalyse);
	}
}

void UForeman_BrainComponent::TickNavigating(float DeltaTime)
{
	// Check if we've arrived
	AForeman_AIController* Controller = GetForemanController();
	if (!Controller) return;

	EPathFollowingStatus::Type Status = Controller->GetMoveStatus();
	if (Status == EPathFollowingStatus::Idle)
	{
		HandleMoveToComplete();
	}
}

void UForeman_BrainComponent::HandleMoveToComplete()
{
	if (CurrentState == EForemanState::NavigatingToTarget)
	{
		// Find and pick up target
		AActor* Target = FindActorByTag(TargetActorTag);
		if (!Target) return;

		// Check distance to ensure we actually reached it
		float DistToCone = FVector::Dist(
			GetOwner()->GetActorLocation(),
			Target->GetActorLocation());

		if (DistToCone > 150.f)
		{
			// Didn't actually reach it, try again
			UE_LOG(LogTemp, Warning, 
				TEXT("Foreman: Didn't reach target, retrying (distance: %.1f)"), DistToCone);
			GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Orange,
				FString::Printf(TEXT("Too far (%.1f), retrying move"), DistToCone));
			AForeman_AIController* Controller = GetForemanController();
			if (Controller)
			{
				Controller->ExecuteMoveToActor(Target);
			}
			return;
		}

		GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Green,
			TEXT("Foreman: Arrived at target"));
		SetState(EForemanState::PickingUp);
		PickUpActor(Target);
	}
	else if (CurrentState == EForemanState::NavigatingToDestination)
	{
		// Validate we're still holding something
		if (!HeldActor)
		{
			UE_LOG(LogTemp, Error,
				TEXT("Foreman: Arrived at destination but holding nothing!"));
			GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red,
				TEXT("Foreman: ERROR - arrived but holding nothing"));
			SetState(EForemanState::Idle);
			return;
		}

		GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Green,
			TEXT("Foreman: Arrived at destination"));
		PlaceHeldActor(TargetLocation);
		SetState(EForemanState::TaskComplete);
		GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Cyan,
			TEXT("Foreman: Task Complete!"));
	}
}

void UForeman_BrainComponent::PickUpActor(AActor* Target)
{
	if (!Target) return;
	AActor* ForemanActor = GetForemanActor();
	if (!ForemanActor) return;

	HeldActor = Target;

	FAttachmentTransformRules Rules(
		EAttachmentRule::SnapToTarget, true);

	Target->AttachToActor(ForemanActor, Rules);
	Target->SetActorRelativeLocation(FVector(30.f, 0.f, 0.f));

	// Verify attachment succeeded
	if (!Target->GetAttachParentActor())
	{
		UE_LOG(LogTemp, Error, 
			TEXT("Foreman: Pickup FAILED - attachment rejected"));
		GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red,
			TEXT("Foreman: Pickup failed - check Mobility"));
		HeldActor = nullptr;
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Foreman: Pickup CONFIRMED - %s attached"),
		*Target->GetName());
	GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Green,
		FString::Printf(TEXT("Foreman: Picked up %s"),
			*Target->GetName()));

	// Now navigate to center
	TargetLocation = FVector::ZeroVector;
	AForeman_AIController* Controller = GetForemanController();
	if (Controller)
	{
		Controller->ExecuteMoveToLocation(TargetLocation);
		SetState(EForemanState::NavigatingToDestination);
	}
}

void UForeman_BrainComponent::PlaceHeldActor(FVector Location)
{
	if (!HeldActor) return;

	HeldActor->DetachFromActor(
		FDetachmentTransformRules::KeepWorldTransform);
	HeldActor->SetActorLocation(Location);
	HeldActor = nullptr;

	UE_LOG(LogTemp, Warning,
		TEXT("Foreman: Placed object at center"));
}

void UForeman_BrainComponent::SnapAndAnalyse()
{
	if (!SceneCapture || !RenderTarget) return;

	bWaitingForLLMResponse = true;
	SceneCapture->CaptureScene();

	FString Base64 = RenderTargetToBase64();
	if (Base64.IsEmpty())
	{
		bWaitingForLLMResponse = false;
		return;
	}

	FString Context = BuildPerceptionContext();
	SendToLLM(Base64, Context);
}

FString UForeman_BrainComponent::BuildPerceptionContext()
{
	LastPerceivedActors.Empty();
	AActor* ForemanActor = GetForemanActor();
	const FVector ForemanLocation = ForemanActor
		? ForemanActor->GetActorLocation()
		: FVector::ZeroVector;

	if (PerceptionComponent)
	{
		PerceptionComponent->RequestStimuliListenerUpdate();
		PerceptionComponent->GetCurrentlyPerceivedActors(
			UAISense_Sight::StaticClass(), LastPerceivedActors);
	}

	// Deterministic fallback: include red_cone-tagged actors even if sight
	// perception has not registered yet this frame.
	TArray<AActor*> TaggedCones;
	UGameplayStatics::GetAllActorsWithTag(
		GetWorld(), FName(TEXT("red_cone")), TaggedCones);

	for (AActor* Actor : TaggedCones)
	{
		if (Actor && !LastPerceivedActors.Contains(Actor))
		{
			LastPerceivedActors.Add(Actor);
		}
	}

	FString Json = FString::Printf(
		TEXT("{\"command\":\"%s\",\"currently_visible\":["),
		*EscapeForJson(CurrentCommand));

	bool bFirst = true;
	for (AActor* Actor : LastPerceivedActors)
	{
		if (!Actor) continue;

		FString Tag = Actor->GetName();
		for (const FName& ActorTag : Actor->Tags)
		{
			const FString TagString = ActorTag.ToString();
			if (TagString.Equals(TEXT("red_cone"), ESearchCase::IgnoreCase))
			{
				Tag = TEXT("red_cone");
				break;
			}

			if (Tag == Actor->GetName())
			{
				Tag = TagString;
			}
		}

		FVector Pos = Actor->GetActorLocation();
		float Dist = FVector::Dist(
			ForemanLocation, Pos);

		if (!bFirst) Json += TEXT(",");
		bFirst = false;

		Json += FString::Printf(
			TEXT("{\"tag\":\"%s\",\"distance\":%.1f,"
				 "\"position\":[%.1f,%.1f,%.1f]}"),
			*EscapeForJson(Tag), Dist, Pos.X, Pos.Y, Pos.Z);
	}

	Json += TEXT("]}");
	return Json;
}

void UForeman_BrainComponent::SendToLLM(const FString& Base64,
	const FString& Context)
{
	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(LMStudioURL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	FString EscapedContext = EscapeForJson(Context);

	FString Body = FString::Printf(TEXT(R"({
		"model": "liquid/lfm2.5-vl-1.6b",
		"messages": [
			{
				"role": "system",
				"content": "You are Kellan, an AI foreman in Unreal Engine. You receive a command and scene context. Return STRICT JSON only: {\"summary\":string,\"target_found\":bool,\"target_tag\":string,\"action\":{\"action\":string,\"target\":string,\"direction\":string,\"speed\":string}}. Valid actions: move_to, pick_up, place, look_at, wait. Never invent objects. Only reference tags from context."
			},
			{
				"role": "user",
				"content": [
					{
						"type": "text",
						"text": "context: %s"
					},
					{
						"type": "image_url",
						"image_url": {
							"url": "data:image/png;base64,%s"
						}
					},
					{
						"type": "text",
						"text": "Execute your current command based on what you see."
					}
				]
			}
		],
		"temperature": 0.1,
		"stream": false,
		"max_tokens": 300
	})"), *EscapedContext, *Base64);

	Request->SetContentAsString(Body);
	Request->OnProcessRequestComplete().BindUObject(this,
		&UForeman_BrainComponent::OnLLMResponse);
	Request->ProcessRequest();
}

void UForeman_BrainComponent::OnLLMResponse(FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful)
{
	check(Request);  // Callback signature requires it, even if unused
	bWaitingForLLMResponse = false;

	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Foreman: LLM request failed"));
		return;
	}

	FString Raw = Response->GetContentAsString();
	TSharedPtr<FJsonObject> Outer;
	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Raw);

	if (!FJsonSerializer::Deserialize(Reader, Outer)) return;

	TArray<TSharedPtr<FJsonValue>> Choices =
		Outer->GetArrayField(TEXT("choices"));
	if (Choices.Num() == 0) return;

	FString Content = Choices[0]->AsObject()
		->GetObjectField(TEXT("message"))
		->GetStringField(TEXT("content"));

	Content = SanitizeJson(Content);

	TSharedPtr<FJsonObject> Inner;
	TSharedRef<TJsonReader<>> InnerReader =
		TJsonReaderFactory<>::Create(Content);

	if (!FJsonSerializer::Deserialize(InnerReader, Inner))
	{
		UE_LOG(LogTemp, Error,
			TEXT("Foreman: JSON parse failed - %s"), *Content);
		return;
	}

	bool bTargetFound = Inner->GetBoolField(TEXT("target_found"));
	FString TargetTag = Inner->GetStringField(TEXT("target_tag"));

	UE_LOG(LogTemp, Warning,
		TEXT("Foreman sees: %s | Target found: %s"),
		*Inner->GetStringField(TEXT("summary")),
		bTargetFound ? TEXT("YES") : TEXT("NO"));

	GEngine->AddOnScreenDebugMessage(-1, 30.f,
		bTargetFound ? FColor::Green : FColor::Yellow,
		FString::Printf(TEXT("Kellan: %s"),
			*Inner->GetStringField(TEXT("summary"))));

	if (bTargetFound && !TargetTag.IsEmpty())
	{
		// Target located - navigate to it
		AActor* Target = FindActorByTag(TargetTag);
		if (Target)
		{
			TargetActorTag = TargetTag;
			AForeman_AIController* Controller = GetForemanController();
			if (Controller)
			{
				Controller->ExecuteMoveToActor(Target);
				SetState(EForemanState::NavigatingToTarget);
				GEngine->AddOnScreenDebugMessage(-1, 30.f,
					FColor::Green,
					FString::Printf(
						TEXT("Kellan: Found %s, moving to it"),
						*TargetTag));
			}
		}
	}
	else if (CurrentState == EForemanState::LookingAround &&
		LookAroundSnapsCount >= 4)
	{
		// Completed full rotation, target not found
		GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Orange,
			TEXT("Kellan: Target not found after full scan"));
		SetState(EForemanState::Idle);
	}
}

FString UForeman_BrainComponent::SanitizeJson(const FString& Raw)
{
	FString Clean = Raw;
	Clean.ReplaceInline(TEXT("}]]"), TEXT("}]"));
	Clean.ReplaceInline(TEXT("}}]"), TEXT("}]"));
	Clean.ReplaceInline(TEXT("```json"), TEXT(""));
	Clean.ReplaceInline(TEXT("```"), TEXT(""));
	Clean.ReplaceInline(TEXT(":float}"), TEXT(":0.0}"));
	Clean.ReplaceInline(TEXT(":float,"), TEXT(":0.0,"));
	Clean.TrimStartAndEndInline();
	return Clean;
}

void UForeman_BrainComponent::OnTargetPerceptionUpdated(
	const FActorPerceptionUpdateInfo& UpdateInfo)
{
	AActor* TargetActor = UpdateInfo.Target.Get();
	if (!TargetActor)
	{
		return;
	}

	const bool bSuccessfullySensed = UpdateInfo.Stimulus.WasSuccessfullySensed();

	// Log perception event
	UE_LOG(LogTemp, Warning,
		TEXT("Foreman: Perceived '%s' (success=%d)"),
		*TargetActor->GetName(),
		(int32)bSuccessfullySensed);

	if (!bSuccessfullySensed)
	{
		// Target lost
		if (TargetActor->ActorHasTag(TEXT("red_cone")))
		{
			PerceivedRedCone = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Foreman: Lost sight of red cone"));
		}
		return;
	}

	// Target detected
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
		FString::Printf(TEXT("DETECTED: %s"), *TargetActor->GetName()));

	// Track red_cone specifically
	if (TargetActor->ActorHasTag(TEXT("red_cone")))
	{
		PerceivedRedCone = TargetActor;
		UE_LOG(LogTemp, Warning, TEXT("Foreman: *** RED CONE DETECTED ***"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			TEXT("*** RED CONE DETECTED ***"));
	}

	// Always add to perceived actors list for LLM context
	if (!LastPerceivedActors.Contains(TargetActor))
	{
		LastPerceivedActors.AddUnique(TargetActor);
	}
}

AActor* UForeman_BrainComponent::FindActorByTag(const FString& Tag)
{
	for (AActor* Actor : LastPerceivedActors)
	{
		if (!Actor) continue;
		FString ActorTag = Actor->Tags.Num() > 0 ?
			Actor->Tags[0].ToString() : Actor->GetName();
		if (ActorTag.Equals(Tag, ESearchCase::IgnoreCase))
			return Actor;
	}

	// Fallback - search whole world
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsWithTag(
		GetWorld(), FName(*Tag), AllActors);
	return AllActors.Num() > 0 ? AllActors[0] : nullptr;
}

AForeman_AIController* UForeman_BrainComponent::GetForemanController()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (AForeman_AIController* Controller = Cast<AForeman_AIController>(Owner))
	{
		return Controller;
	}

	APawn* Pawn = GetForemanPawn();
	return Pawn ? Cast<AForeman_AIController>(Pawn->GetController()) : nullptr;
}

AActor* UForeman_BrainComponent::GetForemanActor() const
{
	APawn* Pawn = GetForemanPawn();
	if (Pawn)
	{
		return Pawn;
	}

	return GetOwner();
}

APawn* UForeman_BrainComponent::GetForemanPawn() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		return OwnerPawn;
	}

	if (AController* OwnerController = Cast<AController>(Owner))
	{
		return OwnerController->GetPawn();
	}

	return nullptr;
}

FString UForeman_BrainComponent::RenderTargetToBase64()
{
	if (!RenderTarget) return FString();
	FRenderTarget* RT =
		RenderTarget->GameThread_GetRenderTargetResource();
	if (!RT) return FString();

	TArray<FColor> Pixels;
	if (!RT->ReadPixels(Pixels)) return FString();

	IImageWrapperModule& IWM =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(
			FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> IW =
		IWM.CreateImageWrapper(EImageFormat::PNG);
	IW->SetRaw(Pixels.GetData(), Pixels.GetAllocatedSize(),
		RenderTarget->SizeX, RenderTarget->SizeY,
		ERGBFormat::BGRA, 8);
	TArray64<uint8> PNG = IW->GetCompressed(0);
	return FBase64::Encode(PNG.GetData(), PNG.Num());
}





