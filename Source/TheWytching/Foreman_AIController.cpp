#include "Foreman_AIController.h"

#include "AndroidConditionComponent.h"
#include "ForemanTypes.h"
#include "Foreman_BrainComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "Perception/AIPerceptionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/ConstructorHelpers.h"

AForeman_AIController::AForeman_AIController()
{
	// StateTree AI Component — drives the Foreman decision tree
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));

	// Default StateTree asset — loaded from content
	static ConstructorHelpers::FObjectFinder<UStateTree> DefaultTreeFinder(
		TEXT("/Game/Foreman/ST_Foreman.ST_Foreman"));
	if (DefaultTreeFinder.Succeeded())
	{
		DefaultStateTree = DefaultTreeFinder.Object;
	}

	// Perception — configured by ForemanBrain (sole owner of perception config)
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("ForemanPerception")));

	// Brain Component — LLM/vision cognitive pipeline
	ForemanBrain = CreateDefaultSubobject<UForeman_BrainComponent>(TEXT("ForemanBrain"));
}

void AForeman_AIController::BeginPlay()
{
	Super::BeginPlay();

	if (ForemanBrain)
	{
		ForemanBrain->RequestBoot();
	}

	// Set the StateTree from the DefaultStateTree property if not already running
	if (StateTreeAI != nullptr && DefaultStateTree != nullptr)
	{
		if (!StateTreeAI->IsRunning())
		{
			StateTreeAI->SetStateTree(DefaultStateTree.Get());
			UE_LOG(LogForeman, Log, TEXT("StateTree set to: %s"), *DefaultStateTree.Get()->GetName());
			StateTreeAI->StartLogic();
			UE_LOG(LogForeman, Log, TEXT("StateTree StartLogic from BeginPlay"));
		}
	}
	else if (StateTreeAI != nullptr && DefaultStateTree == nullptr)
	{
		UE_LOG(LogForeman, Warning, TEXT("No DefaultStateTree assigned in AIController"));
	}
}

void AForeman_AIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ForemanBrain)
	{
		ForemanBrain->RequestBoot();
	}

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
	if (ForemanBrain)
	{
		ForemanBrain->ShutdownBoot();
	}
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

void AForeman_AIController::RegisterWorker(AActor* Worker)
{
	if (!Worker) return;
	// Avoid duplicates
	for (const TWeakObjectPtr<AActor>& Entry : RegisteredWorkers)
	{
		if (Entry.Get() == Worker) return;
	}
	RegisteredWorkers.Add(Worker);
	UE_LOG(LogForeman, Log, TEXT("RegisterWorker: %s — roster size: %d"),
		*Worker->GetName(), RegisteredWorkers.Num());
}

void AForeman_AIController::UnregisterWorker(AActor* Worker)
{
	if (!Worker) return;
	const int32 Before = RegisteredWorkers.Num();
	RegisteredWorkers.RemoveAll([Worker](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Worker;
	});
	UE_LOG(LogForeman, Log, TEXT("UnregisterWorker: %s — roster size: %d -> %d"),
		*Worker->GetName(), Before, RegisteredWorkers.Num());
}

UAndroidConditionComponent* AForeman_AIController::GetOwnCondition() const
{
	if (APawn* OwnedPawn = GetPawn())
	{
		return OwnedPawn->FindComponentByClass<UAndroidConditionComponent>();
	}
	return nullptr;
}
