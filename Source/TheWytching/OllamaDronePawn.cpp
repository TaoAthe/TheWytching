#include "OllamaDronePawn.h"
#include "Camera/CameraComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "WorldCollision.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Foreman_BrainComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

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

AOllamaDronePawn::AOllamaDronePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root box
	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	BoxCollision->SetBoxExtent(FVector(40.f, 40.f, 20.f));
	BoxCollision->SetSimulatePhysics(false);
	RootComponent = BoxCollision;

	// Player camera
	DroneCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DroneCamera"));
	DroneCamera->SetupAttachment(RootComponent);
	DroneCamera->SetRelativeLocation(FVector(0.f, 0.f, 10.f));

	// Scene capture - same orientation as camera
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(
		TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(RootComponent);
	SceneCapture->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	SceneCapture->CaptureSource = SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	// TopDownCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(
	// 	TEXT("TopDownCapture"));
	// TopDownCapture->SetupAttachment(RootComponent);
	// TopDownCapture->CaptureSource = SCS_FinalColorLDR;
	// TopDownCapture->bCaptureEveryFrame = false;
	// TopDownCapture->bCaptureOnMovement = false;
	// TopDownCapture->ProjectionType = ECameraProjectionMode::Orthographic;

	// AIPerception
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(
		TEXT("AIPerception"));

	UAISenseConfig_Sight* SightConfig = 
		CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
	SightConfig->SetMaxAge(5.f); // Remember actors for 5 seconds
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(
		SightConfig->GetSenseImplementation());

	// No gravity
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AOllamaDronePawn::BeginPlay()
{
	Super::BeginPlay();

	// Create render target
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->InitAutoFormat(CaptureSize, CaptureSize);
	RenderTarget->UpdateResourceImmediate(true);
	SceneCapture->TextureTarget = RenderTarget;

	// TopDownRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	// TopDownRenderTarget->InitAutoFormat(CaptureSize, CaptureSize);
	// TopDownRenderTarget->UpdateResourceImmediate(true);
	// TopDownCapture->TextureTarget = TopDownRenderTarget;

	// TopDownCapture->SetRelativeLocation(FVector(0.f, 0.f, TopDownHeight));
	// TopDownCapture->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	// TopDownCapture->OrthoWidth = TopDownOrthoWidth;

	// Disable gravity via physics volume or just zero out
	BoxCollision->SetEnableGravity(false);

	// Debug information
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("Drone: Possessed by PlayerController ✓"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
			TEXT("Drone Possessed by PlayerController ✓"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Drone: NOT possessed by PlayerController ✗"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			TEXT("Drone NOT possessed by PlayerController ✗ - Check GameMode!"));
	}

	UE_LOG(LogTemp, Warning, TEXT("Drone: Ready. WASD to move, QE for height, F to snap."));
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow,
		TEXT("Drone Ready — F to snap and send to LMStudio"));
}

void AOllamaDronePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Apply movement relative to control rotation
	FRotator ControlRot = GetControlRotation();
	FRotator YawOnly(0.f, ControlRot.Yaw, 0.f);

	FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	FVector Up = FVector::UpVector;

	FVector Velocity = (Forward * MovementInput.X)
	                 + (Right * MovementInput.Y)
	                 + (Up * MovementInput.Z);

	AddActorWorldOffset(Velocity * MoveSpeed * DeltaTime, true);

	// if (TopDownCapture)
	// {
	// 	TopDownCapture->SetWorldLocation(
	// 		GetActorLocation() + FVector(0.f, 0.f, TopDownHeight));
	// 	TopDownCapture->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
	// }
}

void AOllamaDronePawn::SetupPlayerInputComponent(
	UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("Drone: PlayerInputComponent is NULL!"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			TEXT("ERROR: PlayerInputComponent is NULL"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Drone: Binding input axes..."));

	PlayerInputComponent->BindAxis("MoveForward", this,
		&AOllamaDronePawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this,
		&AOllamaDronePawn::MoveRight);
	PlayerInputComponent->BindAxis("MoveUp", this,
		&AOllamaDronePawn::MoveUp);
	PlayerInputComponent->BindAxis("Turn", this,
		&AOllamaDronePawn::Turn);
	PlayerInputComponent->BindAxis("LookUp", this,
		&AOllamaDronePawn::LookUp);

	PlayerInputComponent->BindAction("Snap", IE_Pressed, this,
		&AOllamaDronePawn::SnapAndSend);

	PlayerInputComponent->BindAction("IssueForemanCommand", IE_Pressed, this,
		&AOllamaDronePawn::IssueFormanCommand);

	UE_LOG(LogTemp, Warning, TEXT("Drone: Input bindings complete ✓"));
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
		TEXT("Drone Input Bindings Complete ✓"));
}

void AOllamaDronePawn::MoveForward(float Value)
{
	if (FMath::Abs(Value) > 0.1f)
	{
		UE_LOG(LogTemp, Log, TEXT("Drone: MoveForward = %f"), Value);
	}
	MovementInput.X = FMath::Clamp(Value, -1.f, 1.f);
}

void AOllamaDronePawn::MoveRight(float Value)
{
	if (FMath::Abs(Value) > 0.1f)
	{
		UE_LOG(LogTemp, Log, TEXT("Drone: MoveRight = %f"), Value);
	}
	MovementInput.Y = FMath::Clamp(Value, -1.f, 1.f);
}

void AOllamaDronePawn::MoveUp(float Value)
{
	if (FMath::Abs(Value) > 0.1f)
	{
		UE_LOG(LogTemp, Log, TEXT("Drone: MoveUp = %f"), Value);
	}
	MovementInput.Z = FMath::Clamp(Value, -1.f, 1.f);
}

void AOllamaDronePawn::Turn(float Value)
{
	if (FMath::Abs(Value) > 0.1f)
	{
		UE_LOG(LogTemp, Log, TEXT("Drone: Turn = %f"), Value);
	}
	AddControllerYawInput(Value * LookSensitivity);
}

void AOllamaDronePawn::LookUp(float Value)
{
	if (FMath::Abs(Value) > 0.1f)
	{
		UE_LOG(LogTemp, Log, TEXT("Drone: LookUp = %f"), Value);
	}
	AddControllerPitchInput(Value * LookSensitivity);
}

void AOllamaDronePawn::SnapAndSend()
{
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow,
		TEXT("Drone: Snapping..."));

	SceneCapture->CaptureScene();
	FString Base64 = RenderTargetToBase64(RenderTarget);

	if (Base64.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Drone: Capture failed"));
		return;
	}

	// Build perception context
	const FString Context = BuildPerceptionContext();
	
	UE_LOG(LogTemp, Warning, TEXT("Drone Perception Context: %s"), *Context);

	// Send to LMStudio
	SendImageToLLM(Base64, Context);
	
	// Send to Gemini (commented out for now - using LMStudio only)
	// SendImageToGemini(Base64, Context);
}

FString AOllamaDronePawn::RenderTargetToBase64(UTextureRenderTarget2D* Target)
{
	if (!Target) return FString();

	FRenderTarget* RT = Target->GameThread_GetRenderTargetResource();
	if (!RT) return FString();

	TArray<FColor> Pixels;
	if (!RT->ReadPixels(Pixels)) return FString();

	int32 Width = Target->SizeX;
	int32 Height = Target->SizeY;

	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(
			FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	ImageWrapper->SetRaw(Pixels.GetData(),
		Pixels.GetAllocatedSize(), Width, Height,
		ERGBFormat::BGRA, 8);

	TArray64<uint8> PNGData = ImageWrapper->GetCompressed(0);
	return FBase64::Encode(PNGData.GetData(), PNGData.Num());
}

FString AOllamaDronePawn::BuildPerceptionContext()
{
	// Clear and populate perception cache
	LastPerceivedActors.Empty();
	PerceptionComponent->GetCurrentlyPerceivedActors(
		UAISense_Sight::StaticClass(), LastPerceivedActors);

	TArray<AActor*> PerceivedActors;
	PerceptionComponent->GetCurrentlyPerceivedActors(
		UAISense_Sight::StaticClass(), PerceivedActors);

	TArray<AActor*> AllPerceivedActors;
	PerceptionComponent->GetKnownPerceivedActors(
		UAISense_Sight::StaticClass(), AllPerceivedActors);

	FString VisibleJson = TEXT("[");
	bool bFirst = true;

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor) continue;

		// Get tag or fallback to actor name
		FString Tag = Actor->Tags.Num() > 0 ?
			Actor->Tags[0].ToString() : Actor->GetName();

		// Skip noise
		if (Tag.Contains("LevelBlock") || 
			Tag.Contains("Landscape") ||
			Tag.Contains("LevelButton") ||
			Tag.Contains("Teleporter") ||
			Tag.Contains("Floor") ||
			Tag.Contains("Wall"))
			continue;

		FVector ActorLoc = Actor->GetActorLocation();
		FVector DroneLoc = GetActorLocation();
		float Distance = FVector::Dist(DroneLoc, ActorLoc);

		// Get relative direction
		FVector ToActor = (ActorLoc - DroneLoc).GetSafeNormal();
		FVector Forward = GetActorForwardVector();
		FVector Right = GetActorRightVector();

		float DotForward = FVector::DotProduct(Forward, ToActor);
		float DotRight = FVector::DotProduct(Right, ToActor);

		FString Direction;
		if (DotForward > 0.7f) Direction = TEXT("forward");
		else if (DotForward < -0.7f) Direction = TEXT("behind");
		else if (DotRight > 0.f) Direction = TEXT("right");
		else Direction = TEXT("left");

		if (!bFirst) VisibleJson += TEXT(",");
		bFirst = false;

		VisibleJson += FString::Printf(
			TEXT("{\"tag\":\"%s\",\"direction\":\"%s\","
				 "\"distance\":%.1f,"
				 "\"position\":[%.1f,%.1f,%.1f]}"),
			*Tag, *Direction, Distance,
			ActorLoc.X, ActorLoc.Y, ActorLoc.Z);
	}

	VisibleJson += TEXT("]");

	// Recently lost actors
	FString LostJson = TEXT("[");
	bool bFirstLost = true;

	for (AActor* Actor : AllPerceivedActors)
	{
		if (!Actor) continue;
		if (PerceivedActors.Contains(Actor)) continue;

		FString Tag = Actor->Tags.Num() > 0 ?
			Actor->Tags[0].ToString() : Actor->GetName();

		if (Tag.Contains("LevelBlock") || 
			Tag.Contains("Landscape") ||
			Tag.Contains("LevelButton") ||
			Tag.Contains("Teleporter") ||
			Tag.Contains("Floor") ||
			Tag.Contains("Wall"))
			continue;

		FActorPerceptionBlueprintInfo Info;
		PerceptionComponent->GetActorsPerception(Actor, Info);

		float LastSeenAge = 0.f;
		if (Info.LastSensedStimuli.Num() > 0)
		{
			LastSeenAge = Info.LastSensedStimuli[0].GetAge();
		}

		if (!bFirstLost) LostJson += TEXT(",");
		bFirstLost = false;

		FVector LastLoc = Actor->GetActorLocation();
		LostJson += FString::Printf(
			TEXT("{\"tag\":\"%s\",\"last_seen_seconds\":%.1f,"
				 "\"last_position\":[%.1f,%.1f,%.1f]}"),
			*Tag, LastSeenAge,
			LastLoc.X, LastLoc.Y, LastLoc.Z);
	}

	LostJson += TEXT("]");

	return FString::Printf(
		TEXT("{\"currently_visible\":%s,\"recently_lost\":%s}"),
		*VisibleJson, *LostJson);
}

FString AOllamaDronePawn::BuildContextText() const
{
	const FVector Location = GetActorLocation();
	const FRotator Rotation = GetActorRotation();
	const FVector Velocity = GetVelocity();

	const float DistForward = TraceDistance(GetActorForwardVector(), TraceMaxDistance);
	const float DistBackward = TraceDistance(-GetActorForwardVector(), TraceMaxDistance);
	const float DistRight = TraceDistance(GetActorRightVector(), TraceMaxDistance);
	const float DistLeft = TraceDistance(-GetActorRightVector(), TraceMaxDistance);
	const float DistDown = TraceDistance(-GetActorUpVector(), TraceMaxDistance);

	// Ground surface info
	UWorld* World = GetWorld();
	FString SurfaceJson = TEXT("\"surface\":{\"dist\":null,\"slope\":null,\"material\":null}");
	if (World)
	{
		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneGround), false, this);
		const FVector Start = Location;
		const FVector End = Start + FVector(0.f, 0.f, -TraceMaxDistance);
		
		if (World->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, Params))
		{
			const float GroundDist = GroundHit.Distance;
			const FVector Normal = GroundHit.ImpactNormal;
			const float Slope = FMath::RadiansToDegrees(FMath::Acos(Normal | FVector::UpVector));
			FString MatName = TEXT("unknown");
			if (GroundHit.PhysMaterial.IsValid())
			{
				MatName = GroundHit.PhysMaterial->GetName().Replace(TEXT("\""), TEXT("'"));
			}
			SurfaceJson = FString::Printf(
				TEXT("\"surface\":{\"dist\":%.1f,\"slope\":%.1f,\"material\":\"%s\"}"),
				GroundDist, Slope, *MatName);
		}
	}

	// Nearby actors with enriched data
	FString NearbyJson = TEXT("\"nearby\":[");
	
	// Filter tags to ignore structural/landscape actors
	TArray<FString> IgnoreTags = {
		TEXT("Landscape"),
		TEXT("LevelBlock"),
		TEXT("Floor"),
		TEXT("Wall"),
		TEXT("Teleporter")
	};

	TArray<TPair<float, AActor*>> Sorted;

	if (World)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllObjects);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneNearby), false, this);

		const bool bHit = World->OverlapMultiByObjectType(
			Overlaps,
			Location,
			FQuat::Identity,
			ObjectParams,
			FCollisionShape::MakeSphere(NearbyScanRadius),
			Params);

		if (bHit)
		{
			Sorted.Reserve(Overlaps.Num());
			for (const FOverlapResult& Result : Overlaps)
			{
				AActor* Actor = Result.GetActor();
				if (!Actor || Actor == this) continue;

				// Get tag or name
				FString Tag = Actor->Tags.Num() > 0
					? Actor->Tags[0].ToString()
					: Actor->GetName();

				// Skip ignored structural actors
				bool bShouldIgnore = false;
				for (const FString& IgnoreTag : IgnoreTags)
				{
					if (Tag.Contains(IgnoreTag))
					{
						bShouldIgnore = true;
						break;
					}
				}
				if (bShouldIgnore) continue;

				const float Dist = FVector::Dist(Location, Actor->GetActorLocation());
				Sorted.Emplace(Dist, Actor);
			}
		}
	}

	Sorted.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B)
	{
		return A.Key < B.Key;
	});

	// Deduplicate by tag name (keep closest instance of each tag)
	TMap<FString, TPair<float, AActor*>> UniqueActors;
	for (const TPair<float, AActor*>& Item : Sorted)
	{
		AActor* Actor = Item.Value;
		if (!Actor) continue;

		FString Tag = Actor->Tags.Num() > 0
			? Actor->Tags[0].ToString()
			: Actor->GetName();

		// Keep only the first (closest) occurrence of each tag
		if (!UniqueActors.Contains(Tag))
		{
			UniqueActors.Add(Tag, Item);
		}
	}

	// Limit to top 8 meaningful actors
	const int32 MaxActors = 8;
	int32 Added = 0;

	for (const auto& Pair : UniqueActors)
	{
		if (Added >= MaxActors) break;
		
		const FString& Tag = Pair.Key;
		AActor* Actor = Pair.Value.Value;
		const float Dist = Pair.Value.Key;

		if (!Actor) continue;

		const FVector Delta = Actor->GetActorLocation() - Location;
		const FVector Dir = Delta.GetSafeNormal();
		const FString Name = Actor->GetName().Replace(TEXT("\""), TEXT("'"));
		const FString ClassName = Actor->GetClass()->GetName().Replace(TEXT("\""), TEXT("'"));
		const FString CleanTag = Tag.Replace(TEXT("\""), TEXT("'"));
		
		const FVector Extent = Actor->GetComponentsBoundingBox().GetExtent();
		const float Size = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
		
		const FVector ActorVel = Actor->GetVelocity();
		const float Speed = ActorVel.Size();

		if (Added > 0)
		{
			NearbyJson += TEXT(",");
		}

		NearbyJson += FString::Printf(
			TEXT("{\"name\":\"%s\",\"class\":\"%s\",\"tag\":\"%s\",\"dist\":%.1f,\"dir\":[%.2f,%.2f,%.2f],\"size\":%.1f,\"speed\":%.1f}"),
			*Name, *ClassName, *CleanTag, Dist, Dir.X, Dir.Y, Dir.Z, Size, Speed);
		++Added;
	}
	NearbyJson += TEXT("]");

	return FString::Printf(
		TEXT("{\"loc\":[%.1f,%.1f,%.1f],\"rot\":[%.1f,%.1f,%.1f],\"vel\":[%.1f,%.1f,%.1f],\"dist\":{\"f\":%.1f,\"b\":%.1f,\"r\":%.1f,\"l\":%.1f,\"d\":%.1f},%s,%s}"),
		Location.X, Location.Y, Location.Z,
		Rotation.Pitch, Rotation.Yaw, Rotation.Roll,
		Velocity.X, Velocity.Y, Velocity.Z,
		DistForward, DistBackward, DistRight, DistLeft, DistDown,
		*SurfaceJson,
		*NearbyJson);
}

float AOllamaDronePawn::TraceDistance(const FVector& Direction,
                                     float MaxDistance) const
{
	UWorld* World = GetWorld();
	if (!World) return MaxDistance;

	const FVector Start = GetActorLocation();
	const FVector End = Start + (Direction.GetSafeNormal() * MaxDistance);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneTrace), false, this);
	const bool bHit = World->LineTraceSingleByChannel(
		Hit, Start, End, ECC_Visibility, Params);

	return bHit ? Hit.Distance : MaxDistance;
}

void AOllamaDronePawn::SendImageToLLM(const FString& Base64Image,
                                     const FString& ContextText)
{
	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://localhost:1234/v1/chat/completions"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	const FString EscapedContext = EscapeForJson(ContextText);

	FString ContentItems = FString::Printf(
		TEXT(R"({ "type": "text", "text": "context_json: %s" })"), *EscapedContext);

	ContentItems += FString::Printf(
		TEXT(R"(, { "type": "image_url", "image_url": { "url": "data:image/png;base64,%s" } })"),
		*Base64Image);

	ContentItems += TEXT(R"(, { "type": "text", "text": "Return STRICT JSON only using schema: {\"summary\":string, \"tagged_actors\":[{\"tag\":string, \"position\":string, \"distance\":float}], \"raycast\":{\"hit\":bool, \"actor\":string, \"distance\":float}, \"action\":{\"action\":string, \"target\":string, \"direction\":string, \"speed\":string}}. Valid actions: move_to, sit, look_at, pick_up, wait, follow. Valid directions: left, right, center, forward, behind. Valid speeds: walk, jog, run. No markdown, no explanation." })");
	
	FString Body = FString::Printf(TEXT(R"({
		"model": "liquid/lfm2.5-vl-1.6b",
		"messages": [
			{
				"role": "system",
				"content": "You are a scout AI for a game character in Unreal Engine. You receive scene images and context. Describe only what you actually see. Respond with strict JSON only. Never invent objects not visible in the scene."
			},
			{
				"role": "user",
				"content": [
					%s
				]
			}
		],
		"temperature": 0.2,
		"max_tokens": 400,
		"stream": false
	})"), *ContentItems);

	Request->SetContentAsString(Body);
	Request->OnProcessRequestComplete().BindUObject(this,
		&AOllamaDronePawn::OnResponseReceived);
	Request->ProcessRequest();

	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow,
		TEXT("Drone: Sent to LMStudio, waiting..."));
}

FString AOllamaDronePawn::SanitizeJson(const FString& Raw)
{
	FString Clean = Raw;
	
	// Fix double closing brackets
	Clean.ReplaceInline(TEXT("}]]"), TEXT("}]"));
	Clean.ReplaceInline(TEXT("}}]"), TEXT("}]"));
	
	// Strip markdown fences just in case
	Clean.ReplaceInline(TEXT("```json"), TEXT(""));
	Clean.ReplaceInline(TEXT("```"), TEXT(""));
	
	// Fix literal type names the model outputs
	Clean.ReplaceInline(TEXT(":float}"), TEXT(":0.0}"));
	Clean.ReplaceInline(TEXT(":float,"), TEXT(":0.0,"));
	
	Clean.TrimStartAndEndInline();
	return Clean;
}

void AOllamaDronePawn::ExecuteAction(const FString& Action, 
                                     const FString& Target)
{
	UE_LOG(LogTemp, Warning, TEXT("Executing: %s -> %s"), 
		*Action, *Target);

	// Find target actor in perception cache
	AActor* TargetActor = nullptr;
	for (AActor* Actor : LastPerceivedActors)
	{
		if (!Actor) continue;
		FString Tag = Actor->Tags.Num() > 0 ?
			Actor->Tags[0].ToString() : Actor->GetName();
		if (Tag.Equals(Target, ESearchCase::IgnoreCase))
		{
			TargetActor = Actor;
			break;
		}
	}

	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, 
			TEXT("ExecuteAction: Target '%s' not in perception cache"),
			*Target);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange,
			FString::Printf(TEXT("Target not visible: %s"), *Target));
		return;
	}

	FVector TargetPos = TargetActor->GetActorLocation();

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
		FString::Printf(TEXT("Action: %s -> %s at [%.0f, %.0f, %.0f]"),
			*Action, *Target, 
			TargetPos.X, TargetPos.Y, TargetPos.Z));

	// Log confirmed position for next phase
	UE_LOG(LogTemp, Warning, 
		TEXT("Target resolved: %s at %s"), 
		*Target, *TargetPos.ToString());
}

void AOllamaDronePawn::SendImageToGemini(const FString& Base64Image,
                                         const FString& ContextText)
{
	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	
	// Gemini API key
	FString GeminiKey = TEXT("AIzaSyBFdq7fWydHIHhOZKt4mfeqYWbLMThA2u0");

	FString URL = FString::Printf(
		TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=%s"),
		*GeminiKey);

	Request->SetURL(URL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	const FString EscapedContext = EscapeForJson(ContextText);

	FString Body = FString::Printf(TEXT(R"({
		"contents": [
			{
				"parts": [
					{
						"text": "You are a scout AI in Unreal Engine. Scene context: %s\n\nDescribe what you see and return STRICT JSON only using schema: {\"summary\":string, \"tagged_actors\":[{\"tag\":string, \"position\":string, \"distance\":float}], \"raycast\":{\"hit\":bool, \"actor\":string, \"distance\":float}, \"action\":{\"action\":string, \"target\":string, \"direction\":string, \"speed\":string}}. Valid actions: move_to, sit, look_at, pick_up, wait, follow. No markdown."
					},
					{
						"inline_data": {
							"mime_type": "image/png",
							"data": "%s"
						}
					}
				]
			}
		],
		"generationConfig": {
			"temperature": 0.1,
			"maxOutputTokens": 800
		}
	})"), *EscapedContext, *Base64Image);

	Request->SetContentAsString(Body);
	Request->OnProcessRequestComplete().BindUObject(this,
		&AOllamaDronePawn::OnGeminiResponseReceived);
	Request->ProcessRequest();

	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow,
		TEXT("Drone: Sent to Gemini, waiting..."));
}

void AOllamaDronePawn::OnResponseReceived(FHttpRequestPtr Request,
                                          FHttpResponsePtr Response,
                                          bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
			TEXT("Drone: LMStudio connection failed"));
		return;
	}

	FString Raw = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("Raw Response: %s"), *Raw);

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

			// Sanitize JSON before parsing
			Content = SanitizeJson(Content);

			// Log full response
			UE_LOG(LogTemp, Warning, TEXT("Drone Vision Response: %s"), *Content);

			// Parse the LLM's JSON response
			TSharedPtr<FJsonObject> LLMJson;
			TSharedRef<TJsonReader<>> LLMReader = TJsonReaderFactory<>::Create(Content);
			
			if (FJsonSerializer::Deserialize(LLMReader, LLMJson))
			{
				// Extract key fields
				FString Summary = LLMJson->GetStringField(TEXT("summary"));
				
				// Get action recommendation
				TSharedPtr<FJsonObject> ActionObj = LLMJson->GetObjectField(TEXT("action"));
				FString Action = ActionObj->GetStringField(TEXT("action"));
				FString Target = ActionObj->GetStringField(TEXT("target"));
				FString Direction = ActionObj->GetStringField(TEXT("direction"));
				FString Speed = ActionObj->GetStringField(TEXT("speed"));

				// Get nearby actors count
				TArray<TSharedPtr<FJsonValue>> TaggedActors = LLMJson->GetArrayField(TEXT("tagged_actors"));
				int32 ActorCount = TaggedActors.Num();

				// Display summary on screen
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Cyan,
					FString::Printf(TEXT("📷 Summary: %s"), *Summary));
				
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green,
					FString::Printf(TEXT("🎯 Action: %s | Target: %s | Dir: %s | Speed: %s"), 
						*Action, *Target, *Direction, *Speed));
				
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow,
					FString::Printf(TEXT("📍 Detected %d nearby actors"), ActorCount));

				// Execute the action
				if (!Action.IsEmpty() && !Target.IsEmpty())
				{
					ExecuteAction(Action, Target);
				}

				// Log nearest 3 actors
				if (ActorCount > 0)
				{
					for (int32 i = 0; i < FMath::Min(3, ActorCount); ++i)
					{
						TSharedPtr<FJsonObject> ActorObj = TaggedActors[i]->AsObject();
						FString Tag = ActorObj->GetStringField(TEXT("tag"));
						float Distance = ActorObj->GetNumberField(TEXT("distance"));
						
						UE_LOG(LogTemp, Log, TEXT("  - %s at %.1f units"), *Tag, Distance);
					}
				}
			}
			else
			{
				// Fallback if JSON parsing fails
				GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Orange,
					FString::Printf(TEXT("👁 %s"), *Content));
			}
		}
	}
}

void AOllamaDronePawn::OnGeminiResponseReceived(FHttpRequestPtr Request,
                                                FHttpResponsePtr Response,
                                                bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
			TEXT("Drone: Gemini connection failed"));
		return;
	}

	FString Raw = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("Gemini Raw Response: %s"), *Raw);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);

	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		// Gemini format: candidates[0].content.parts[0].text
		TArray<TSharedPtr<FJsonValue>> Candidates =
			JsonObject->GetArrayField(TEXT("candidates"));

		if (Candidates.Num() > 0)
		{
			FString Content = Candidates[0]->AsObject()
				->GetObjectField(TEXT("content"))
				->GetArrayField(TEXT("parts"))[0]->AsObject()
				->GetStringField(TEXT("text"));

			// Strip markdown code fences if Gemini adds them
			Content.ReplaceInline(TEXT("```json"), TEXT(""));
			Content.ReplaceInline(TEXT("```"), TEXT(""));
			Content.TrimStartAndEndInline();

			UE_LOG(LogTemp, Warning, TEXT("Gemini Vision Response: %s"), *Content);

			// Display on HUD in light green with wrapping
			// Light green color
			FColor LightGreen(144, 238, 144);
			
			// Split into lines for wrapping (max ~80 chars per line for readability)
			TArray<FString> Lines;
			const int32 MaxCharsPerLine = 80;
			
			// Add header
			Lines.Add(TEXT("=== GEMINI RESPONSE ==="));
			
			// Wrap content
			FString RemainingText = Content;
			while (RemainingText.Len() > 0)
			{
				if (RemainingText.Len() <= MaxCharsPerLine)
				{
					Lines.Add(RemainingText);
					break;
				}
				
				// Find last space before max length
				int32 WrapPos = MaxCharsPerLine;
				for (int32 i = MaxCharsPerLine; i > 0; --i)
				{
					if (RemainingText[i] == ' ' || RemainingText[i] == ',' || RemainingText[i] == ':')
					{
						WrapPos = i + 1;
						break;
					}
				}
				
				Lines.Add(RemainingText.Left(WrapPos).TrimEnd());
				RemainingText = RemainingText.Mid(WrapPos).TrimStart();
			}
			
			Lines.Add(TEXT("======================"));
			
			// Display all lines on screen (reverse order so they appear top-down)
			for (int32 i = Lines.Num() - 1; i >= 0; --i)
			{
				GEngine->AddOnScreenDebugMessage(-1, 30.f, LightGreen, Lines[i]);
			}
		}
	}
}

void AOllamaDronePawn::IssueFormanCommand()
{
	// Find Kellan (Foreman character)
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(),
		AActor::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		class UForeman_BrainComponent* Brain =
			Actor->FindComponentByClass<class UForeman_BrainComponent>();

		// Backward-compatible fallback: some setups host the brain on AIController.
		if (!Brain)
		{
			if (APawn* Pawn = Cast<APawn>(Actor))
			{
				if (AController* PawnController = Pawn->GetController())
				{
					Brain = PawnController->FindComponentByClass<class UForeman_BrainComponent>();
				}
			}
		}

		if (Brain)
		{
			Brain->IssueCommand(
				TEXT("find the red_cone and place it at 0,0,0"));
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
				TEXT("Drone: Command issued to Kellan"));
			return;
		}
	}
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
		TEXT("Drone: Kellan not found in level"));
}

