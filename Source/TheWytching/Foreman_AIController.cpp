#include "Foreman_AIController.h"

#include "ForemanTypes.h"
#include "Foreman_BrainComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"

AForeman_AIController::AForeman_AIController()
{
	// StateTree AI Component — drives the Foreman decision tree
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));

	// DefaultStateTree will be set in the Blueprint or via CDO
	// The actual StateTree asset assignment happens in BeginPlay or via editor

	// Perception — configured in ConfigurePerception() after construction
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("ForemanPerception")));

	// Brain Component — LLM/vision cognitive pipeline
	ForemanBrain = CreateDefaultSubobject<UForeman_BrainComponent>(TEXT("ForemanBrain"));
}

void AForeman_AIController::ConfigurePerception()
{
	UAIPerceptionComponent* Perception = GetPerceptionComponent();
	if (!Perception)
	{
		return;
	}

	// Sight sense
	UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(Perception);
	SightConfig->SightRadius = 3000.f;
	SightConfig->LoseSightRadius = 3500.f;
	SightConfig->PeripheralVisionAngleDegrees = 90.f;
	SightConfig->SetMaxAge(10.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());

	UE_LOG(LogForeman, Log, TEXT("Perception configured: sight radius=%.0f"), SightConfig->SightRadius);
}

void AForeman_AIController::BeginPlay()
{
	Super::BeginPlay();

	// Set the StateTree from the DefaultStateTree property if set
	if (StateTreeAI != nullptr && DefaultStateTree != nullptr)
	{
		StateTreeAI->SetStateTree(DefaultStateTree.Get());
		UE_LOG(LogForeman, Log, TEXT("StateTree set to: %s"), *DefaultStateTree.Get()->GetName());
	}
	else if (StateTreeAI != nullptr && DefaultStateTree == nullptr)
	{
		UE_LOG(LogForeman, Warning, TEXT("No DefaultStateTree assigned in AIController"));
	}
}

void AForeman_AIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Ensure walking movement mode
	if (ACharacter* PossessedCharacter = Cast<ACharacter>(InPawn))
	{
		if (UCharacterMovementComponent* MoveComp = PossessedCharacter->GetCharacterMovement())
		{
			if (MoveComp->MovementMode == MOVE_None)
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
	}

	// Configure perception now that we have a pawn
	ConfigurePerception();


	// Diagnostic: verify runtime class matches schema expectations
	UE_LOG(LogForeman, Warning, TEXT("ControllerClass: %s"), *GetClass()->GetPathName());
	UE_LOG(LogForeman, Warning, TEXT("PawnClass: %s"),
		GetPawn() ? *GetPawn()->GetClass()->GetPathName() : TEXT("NoPawn"));

	// Explicitly start the StateTree with context
	if (StateTreeAI != nullptr)
	{
		if (!StateTreeAI->IsRunning())
		{
			UE_LOG(LogForeman, Warning, TEXT("StateTree not running after Super::OnPossess — forcing StartLogic"));
			StateTreeAI->StartLogic();
		}
	}

	UE_LOG(LogForeman, Warning, TEXT("AIController possessed %s | StateTreeAI=%s  Running=%s"),
		InPawn ? *InPawn->GetName() : TEXT("None"),
		(StateTreeAI != nullptr) ? TEXT("Valid") : TEXT("NULL"),
		(StateTreeAI != nullptr && StateTreeAI->IsRunning()) ? TEXT("Yes") : TEXT("No"));
}

void AForeman_AIController::OnUnPossess()
{
	UE_LOG(LogForeman, Log, TEXT("AIController unpossessing"));
	Super::OnUnPossess();
}

void AForeman_AIController::ExecuteMoveToLocation(FVector Destination)
{
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(Destination);
	MoveRequest.SetAcceptanceRadius(50.f);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);

	EPathFollowingRequestResult::Type Result = MoveTo(MoveRequest);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		FAIMoveRequest FallbackRequest = MoveRequest;
		FallbackRequest.SetUsePathfinding(false);
		Result = MoveTo(FallbackRequest);
	}

	UE_LOG(LogForeman, Log, TEXT("MoveToLocation result=%d destination=%s"),
		static_cast<int32>(Result), *Destination.ToString());
}

void AForeman_AIController::ExecuteMoveToActor(AActor* Target)
{
	if (!Target)
	{
		UE_LOG(LogForeman, Warning, TEXT("ExecuteMoveToActor called with null target"));
		return;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(Target);
	MoveRequest.SetAcceptanceRadius(50.f);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);

	EPathFollowingRequestResult::Type Result = MoveTo(MoveRequest);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		FAIMoveRequest FallbackRequest = MoveRequest;
		FallbackRequest.SetUsePathfinding(false);
		Result = MoveTo(FallbackRequest);
	}

	UE_LOG(LogForeman, Log, TEXT("MoveToActor result=%d target=%s"),
		static_cast<int32>(Result), *Target->GetName());
}
