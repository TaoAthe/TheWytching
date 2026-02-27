#include "AndroidConditionComponent.h"
#include "AndroidTypes.h"
#include "TimerManager.h"

UAndroidConditionComponent::UAndroidConditionComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // power drain uses timer, not tick
}

void UAndroidConditionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Generate personality seed if not set
	if (PersonalitySeed == 0)
	{
		PersonalitySeed = FMath::Rand();
	}

	// Initialize active capabilities from base
	ActiveCapabilities = BaseCapabilities;

	// Start power drain timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PowerDrainTimerHandle,
			this,
			&UAndroidConditionComponent::DrainPower,
			PowerDrainInterval,
			true // looping
		);
	}

	UE_LOG(LogWytchAndroid, Log, TEXT("%s: ConditionComponent initialized — Power=%.2f, Capabilities=%s, Seed=%u"),
		*GetOwner()->GetName(),
		PowerLevel,
		*BaseCapabilities.ToStringSimple(),
		PersonalitySeed);
}

void UAndroidConditionComponent::DrainPower()
{
	if (PowerState == EAndroidPowerState::Dead)
	{
		return;
	}

	PowerLevel = FMath::Clamp(PowerLevel - (PowerDrainRate * PowerDrainInterval), 0.0f, 1.0f);
	UpdatePowerState();
}

void UAndroidConditionComponent::UpdatePowerState()
{
	EAndroidPowerState OldState = PowerState;
	EAndroidPowerState NewState;

	if (PowerLevel <= 0.0f)
	{
		NewState = EAndroidPowerState::Dead;
	}
	else if (PowerLevel <= 0.1f)
	{
		NewState = EAndroidPowerState::Critical;
	}
	else if (PowerLevel <= 0.3f)
	{
		NewState = EAndroidPowerState::Low;
	}
	else
	{
		NewState = EAndroidPowerState::Normal;
	}

	if (NewState != OldState)
	{
		PowerState = NewState;
		OnPowerStateChanged.Broadcast(OldState, NewState);

		UE_LOG(LogWytchAndroid, Log, TEXT("%s: Power state changed %s → %s (level=%.2f)"),
			*GetOwner()->GetName(),
			*UEnum::GetValueAsString(OldState),
			*UEnum::GetValueAsString(NewState),
			PowerLevel);
	}
}

void UAndroidConditionComponent::SetSubsystemStatus(ESubsystemType System, ESubsystemStatus NewStatus)
{
	ESubsystemStatus& StatusRef = GetSubsystemStatusRef(System);
	ESubsystemStatus OldStatus = StatusRef;

	if (OldStatus == NewStatus)
	{
		return;
	}

	StatusRef = NewStatus;
	OnSubsystemChanged.Broadcast(System, OldStatus, NewStatus);
	RecalculateCapabilities();

	UE_LOG(LogWytchAndroid, Log, TEXT("%s: Subsystem %s changed %s → %s"),
		*GetOwner()->GetName(),
		*UEnum::GetValueAsString(System),
		*UEnum::GetValueAsString(OldStatus),
		*UEnum::GetValueAsString(NewStatus));
}

ESubsystemStatus UAndroidConditionComponent::GetSubsystemStatus(ESubsystemType System) const
{
	// const_cast to reuse the mutable helper — safe because we only read
	return const_cast<UAndroidConditionComponent*>(this)->GetSubsystemStatusRef(System);
}

ESubsystemStatus& UAndroidConditionComponent::GetSubsystemStatusRef(ESubsystemType System)
{
	switch (System)
	{
	case ESubsystemType::Vision:			return VisionStatus;
	case ESubsystemType::Audio:				return AudioStatus;
	case ESubsystemType::Locomotion:		return LocomotionStatus;
	case ESubsystemType::ManipulatorLeft:	return ManipulatorLeft;
	case ESubsystemType::ManipulatorRight:	return ManipulatorRight;
	case ESubsystemType::CommLink:			return CommLink;
	default:								return VisionStatus; // fallback, should never hit
	}
}

void UAndroidConditionComponent::RecalculateCapabilities()
{
	// Start from base capabilities
	FGameplayTagContainer NewCapabilities = BaseCapabilities;

	// Remove capabilities that depend on destroyed subsystems.
	// Degraded subsystems keep capabilities but may affect quality (handled by StateTree).
	// Destroyed subsystems lose associated capabilities entirely.

	// Manipulators destroyed → lose Building, Hauling
	if (ManipulatorLeft == ESubsystemStatus::Destroyed && ManipulatorRight == ESubsystemStatus::Destroyed)
	{
		NewCapabilities.RemoveTag(FGameplayTag::RequestGameplayTag(FName("Capability.Building")));
		NewCapabilities.RemoveTag(FGameplayTag::RequestGameplayTag(FName("Capability.Hauling")));
	}

	// Locomotion destroyed → lose everything requiring movement
	if (LocomotionStatus == ESubsystemStatus::Destroyed)
	{
		NewCapabilities.RemoveTag(FGameplayTag::RequestGameplayTag(FName("Capability.Hauling")));
		NewCapabilities.RemoveTag(FGameplayTag::RequestGameplayTag(FName("Capability.Patrol")));
	}

	// Vision destroyed → lose LaserCutter (needs targeting), Combat
	if (VisionStatus == ESubsystemStatus::Destroyed)
	{
		NewCapabilities.RemoveTag(FGameplayTag::RequestGameplayTag(FName("Capability.LaserCutter")));
		NewCapabilities.RemoveTag(FGameplayTag::RequestGameplayTag(FName("Capability.Combat")));
	}

	if (NewCapabilities != ActiveCapabilities)
	{
		ActiveCapabilities = NewCapabilities;

		UE_LOG(LogWytchAndroid, Log, TEXT("%s: Capabilities recalculated → %s"),
			*GetOwner()->GetName(),
			*ActiveCapabilities.ToStringSimple());
	}
}

EAndroidReadiness UAndroidConditionComponent::GetReadiness() const
{
	// Dead = disabled
	if (PowerState == EAndroidPowerState::Dead)
	{
		return EAndroidReadiness::Disabled;
	}

	// Any destroyed subsystem or critical power = needs maintenance
	if (PowerState == EAndroidPowerState::Critical ||
		VisionStatus == ESubsystemStatus::Destroyed ||
		AudioStatus == ESubsystemStatus::Destroyed ||
		LocomotionStatus == ESubsystemStatus::Destroyed ||
		ManipulatorLeft == ESubsystemStatus::Destroyed ||
		ManipulatorRight == ESubsystemStatus::Destroyed ||
		CommLink == ESubsystemStatus::Destroyed)
	{
		return EAndroidReadiness::NeedsMaintenance;
	}

	// Any degraded subsystem or low power = degraded
	if (PowerState == EAndroidPowerState::Low ||
		VisionStatus == ESubsystemStatus::Degraded ||
		AudioStatus == ESubsystemStatus::Degraded ||
		LocomotionStatus == ESubsystemStatus::Degraded ||
		ManipulatorLeft == ESubsystemStatus::Degraded ||
		ManipulatorRight == ESubsystemStatus::Degraded ||
		CommLink == ESubsystemStatus::Degraded)
	{
		return EAndroidReadiness::Degraded;
	}

	// Locomotion immobile = disabled regardless of other subsystems
	if (LocomotionStatus == ESubsystemStatus::Destroyed)
	{
		return EAndroidReadiness::Disabled;
	}

	return EAndroidReadiness::FullyOperational;
}
