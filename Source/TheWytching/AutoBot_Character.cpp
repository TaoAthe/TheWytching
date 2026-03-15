#include "AutoBot_Character.h"

#include "AndroidConditionComponent.h"
#include "IWytchWorkSite.h"
#include "Foreman_AIController.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectRequestTypes.h"
#include "AIController.h"
#include "TimerManager.h"
#include "StructView.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────

AAutoBot_Character::AAutoBot_Character()
{
	PrimaryActorTick.bCanEverTick = false; // state driven by timers/callbacks — no per-frame tick needed

	ConditionComponent = CreateDefaultSubobject<UAndroidConditionComponent>(TEXT("AndroidCondition"));
}

// ─────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────

void AAutoBot_Character::BeginPlay()
{
	Super::BeginPlay();
	// NOTE: Super::BeginPlay() fires all component BeginPlay callbacks,
	// including UAndroidConditionComponent::BeginPlay which sets
	// ActiveCapabilities = BaseCapabilities and starts the power drain timer.
	// We append SpecialistCapabilities AFTER Super so we layer on top of that.

	if (ConditionComponent)
	{
		ConditionComponent->BaseCapabilities.AppendTags(SpecialistCapabilities);
		ConditionComponent->RecalculateCapabilities();
	}

	RegisterWithForeman();

	UE_LOG(LogWytchWorker, Log,
		TEXT("AutoBot [%s] class=%d ready — capabilities: %s"),
		*GetName(),
		(int32)AutoBotClass,
		ConditionComponent ? *ConditionComponent->ActiveCapabilities.ToStringSimple() : TEXT("none"));
}

// ─────────────────────────────────────────────────────────
// RegisterWithForeman
// Finds the first AForeman_AIController in the world and registers self.
// Safe to call at BeginPlay — Foreman controller is expected to exist.
// If the Foreman isn't ready yet, BP_FOREMAN_V2's BeginPlay can call
// RegisterWorker(Self) as a fallback.
// ─────────────────────────────────────────────────────────

void AAutoBot_Character::RegisterWithForeman()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Foreman_AIController lives on the Foreman pawn — find it via world
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(World, AForeman_AIController::StaticClass(), Controllers);

	if (Controllers.IsEmpty())
	{
		UE_LOG(LogWytchWorker, Warning,
			TEXT("AutoBot [%s] RegisterWithForeman: no AForeman_AIController found — will not be in roster"),
			*GetName());
		return;
	}

	// Use the first Foreman found (single-Foreman assumption — revisit for multi-Foreman)
	if (AForeman_AIController* ForemanAIC = Cast<AForeman_AIController>(Controllers[0]))
	{
		ForemanAIC->RegisterWorker(this);
		UE_LOG(LogWytchWorker, Log,
			TEXT("AutoBot [%s] registered with Foreman [%s]"),
			*GetName(), *ForemanAIC->GetName());
	}
}

void AAutoBot_Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ─────────────────────────────────────────────────────────
// IWytchCommandable — ReceiveSmartObjectAssignment
// ─────────────────────────────────────────────────────────

void AAutoBot_Character::ReceiveSmartObjectAssignment_Implementation(
	FSmartObjectClaimHandle ClaimHandle,
	FSmartObjectSlotHandle SlotHandle,
	AActor* TargetActor)
{
	if (!SlotHandle.IsValid())
	{
		UE_LOG(LogWytchWorker, Warning,
			TEXT("AutoBot [%s] ReceiveSmartObjectAssignment: invalid SlotHandle — ignoring"),
			*GetName());
		return;
	}

	if (WorkerState != EWorkerState::Idle)
	{
		UE_LOG(LogWytchWorker, Warning,
			TEXT("AutoBot [%s] ReceiveSmartObjectAssignment: already busy (state=%d) — ignoring"),
			*GetName(), (int32)WorkerState);
		return;
	}

	PendingSlotHandle = SlotHandle;
	ActiveClaimHandle = FSmartObjectClaimHandle::InvalidHandle;
	TargetWorkActor = TargetActor;

	UE_LOG(LogWytchWorker, Log,
		TEXT("AutoBot [%s] received assignment — navigating to slot"),
		*GetName());

	BeginNavigationToSlot();
}

// ─────────────────────────────────────────────────────────
// IWytchCommandable — GetWorkerState
// ─────────────────────────────────────────────────────────

EWorkerState AAutoBot_Character::GetWorkerState_Implementation() const
{
	if (ConditionComponent &&
		ConditionComponent->GetReadiness() == EAndroidReadiness::Disabled)
	{
		return EWorkerState::Unavailable;
	}
	return WorkerState;
}

// ─────────────────────────────────────────────────────────
// IWytchCommandable — GetCapabilities
// ─────────────────────────────────────────────────────────

FGameplayTagContainer AAutoBot_Character::GetCapabilities_Implementation() const
{
	if (ConditionComponent)
	{
		return ConditionComponent->ActiveCapabilities;
	}
	return SpecialistCapabilities;
}

// ─────────────────────────────────────────────────────────
// IWytchCommandable — AbortCurrentTask
// ─────────────────────────────────────────────────────────

void AAutoBot_Character::AbortCurrentTask_Implementation(EAbortReason Reason)
{
	UE_LOG(LogWytchWorker, Log,
		TEXT("AutoBot [%s] AbortCurrentTask reason=%d state=%d"),
		*GetName(), (int32)Reason, (int32)WorkerState);

	GetWorldTimerManager().ClearTimer(ArrivalCheckTimerHandle);

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	if (ActiveClaimHandle.IsValid())
	{
		if (USmartObjectSubsystem* SOSub = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			SOSub->MarkSlotAsFree(ActiveClaimHandle);
		}
	}

	ActiveClaimHandle = FSmartObjectClaimHandle::InvalidHandle;
	PendingSlotHandle = FSmartObjectSlotHandle();
	TargetWorkActor = nullptr;
	WorkerState = EWorkerState::Idle;
}

// ─────────────────────────────────────────────────────────
// BeginNavigationToSlot
// ─────────────────────────────────────────────────────────

void AAutoBot_Character::BeginNavigationToSlot()
{
	USmartObjectSubsystem* SOSub = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!SOSub)
	{
		UE_LOG(LogWytchWorker, Warning,
			TEXT("AutoBot [%s] BeginNavigationToSlot: no SmartObjectSubsystem"),
			*GetName());
		WorkerState = EWorkerState::Idle;
		return;
	}

	TOptional<FTransform> SlotTransform = SOSub->GetSlotTransform(PendingSlotHandle);
	if (!SlotTransform.IsSet())
	{
		UE_LOG(LogWytchWorker, Warning,
			TEXT("AutoBot [%s] BeginNavigationToSlot: no slot transform"),
			*GetName());
		WorkerState = EWorkerState::Idle;
		return;
	}

	SlotDestination = SlotTransform.GetValue().GetLocation();
	WorkerState = EWorkerState::MovingToTask;

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->MoveToLocation(SlotDestination, ArrivalAcceptanceRadius,
			/*bStopOnOverlap=*/true,
			/*bUsePathfinding=*/true,
			/*bProjectDestinationToNavigation=*/true);
	}

	GetWorldTimerManager().SetTimer(
		ArrivalCheckTimerHandle,
		this,
		&AAutoBot_Character::OnArrivedAtSlot,
		ArrivalCheckInterval,
		/*bLoop=*/true);

	UE_LOG(LogWytchWorker, Log,
		TEXT("AutoBot [%s] moving to (%.0f, %.0f, %.0f)"),
		*GetName(), SlotDestination.X, SlotDestination.Y, SlotDestination.Z);
}

// ─────────────────────────────────────────────────────────
// OnArrivedAtSlot — timer-driven arrival check
// ─────────────────────────────────────────────────────────

void AAutoBot_Character::OnArrivedAtSlot()
{
	if (WorkerState != EWorkerState::MovingToTask) return;

	if (FVector::DistSquared(GetActorLocation(), SlotDestination) >
		FMath::Square(ArrivalAcceptanceRadius * 1.5f))
	{
		return; // not there yet
	}

	GetWorldTimerManager().ClearTimer(ArrivalCheckTimerHandle);

	USmartObjectSubsystem* SOSub = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!SOSub)
	{
		ReleaseAndReturnToIdle();
		return;
	}

	// Claim the slot on arrival — worker holds claim (DEC-004)
	// UE 5.7 API: MarkSlotAsClaimed(SlotHandle, ClaimPriority, UserData)
	ActiveClaimHandle = SOSub->MarkSlotAsClaimed(
		PendingSlotHandle,
		ESmartObjectClaimPriority::Normal,
		FConstStructView());
	if (!ActiveClaimHandle.IsValid())
	{
		UE_LOG(LogWytchWorker, Warning,
			TEXT("AutoBot [%s] OnArrivedAtSlot: claim failed — returning to Idle"),
			*GetName());
		ReleaseAndReturnToIdle();
		return;
	}

	WorkerState = EWorkerState::Working;
	UE_LOG(LogWytchWorker, Log, TEXT("AutoBot [%s] claimed slot — Working"), *GetName());

	if (IsValid(TargetWorkActor))
	{
		if (IWytchWorkSite* WorkSite = Cast<IWytchWorkSite>(TargetWorkActor.Get()))
		{
			WorkSite->Execute_BeginWork(TargetWorkActor.Get(), this);
		}
	}
}

// ─────────────────────────────────────────────────────────
// ReleaseAndReturnToIdle
// ─────────────────────────────────────────────────────────

void AAutoBot_Character::ReleaseAndReturnToIdle()
{
	GetWorldTimerManager().ClearTimer(ArrivalCheckTimerHandle);

	if (ActiveClaimHandle.IsValid())
	{
		if (USmartObjectSubsystem* SOSub = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			SOSub->MarkSlotAsFree(ActiveClaimHandle);
		}
		ActiveClaimHandle = FSmartObjectClaimHandle::InvalidHandle;
	}

	PendingSlotHandle = FSmartObjectSlotHandle();
	TargetWorkActor = nullptr;
	WorkerState = EWorkerState::Idle;

	UE_LOG(LogWytchWorker, Log, TEXT("AutoBot [%s] → Idle"), *GetName());
}
