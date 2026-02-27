#include "Foreman_Character.h"

#include "ForemanTypes.h"
#include "Components/CapsuleComponent.h"
#include "Foreman_AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"

AForeman_Character::AForeman_Character()
{
	AIControllerClass = AForeman_AIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	// Set up capsule
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCapsuleRadius(42.f);
		Capsule->SetCapsuleHalfHeight(88.f);
	}

	// Set up skeletal mesh (using UE5 Mannequin as default)
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
			TEXT("SkeletalMesh'/Game/Characters/Mannequins/Humanoids/SK_Mannequin.SK_Mannequin'"));
		if (MeshAsset.Succeeded())
		{
			SkeletalMesh->SetSkeletalMesh(MeshAsset.Object);
			SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SkeletalMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
			UE_LOG(LogForeman, Log, TEXT("SK_Mannequin mesh loaded successfully"));
		}
		else
		{
			UE_LOG(LogForeman, Warning, TEXT("Failed to load SK_Mannequin mesh"));
		}
	}

	// Set up character movement
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->GravityScale = 1.0f;
		MoveComp->DefaultLandMovementMode = MOVE_Walking;
		MoveComp->MaxWalkSpeed = 600.f;
		MoveComp->MaxAcceleration = 1024.f;
		MoveComp->BrakingDecelerationWalking = 2048.f;
		MoveComp->bRunPhysicsWithNoController = true;
		MoveComp->bUseControllerDesiredRotation = true;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator(0.f, 500.f, 0.f);
	}

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// BrainComponent now lives on AForeman_AIController

	// Initialize Ability System Component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void AForeman_Character::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (MoveComp->MovementMode == MOVE_None)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
	}

	if (!Controller)
	{
		SpawnDefaultController();
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		UE_LOG(LogForeman, Log, TEXT("AbilitySystemComponent initialized"));
	}
}

UAbilitySystemComponent* AForeman_Character::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
