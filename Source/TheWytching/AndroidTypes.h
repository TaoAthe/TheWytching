#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AndroidTypes.generated.h"

// ── Android Log Category ──
DECLARE_LOG_CATEGORY_EXTERN(LogWytchAndroid, Log, All);

// ── Worker Log Category ──
DECLARE_LOG_CATEGORY_EXTERN(LogWytchWorker, Log, All);

// ─────────────────────────────────────────────────────────
// EWorkerState — what the android is currently doing
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EWorkerState : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	MovingToTask	UMETA(DisplayName = "Moving To Task"),
	Working			UMETA(DisplayName = "Working"),
	Returning		UMETA(DisplayName = "Returning"),
	Unavailable		UMETA(DisplayName = "Unavailable")
};

// ─────────────────────────────────────────────────────────
// EAndroidPowerState — coarse power level bracket
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EAndroidPowerState : uint8
{
	Normal		UMETA(DisplayName = "Normal"),
	Low			UMETA(DisplayName = "Low"),
	Critical	UMETA(DisplayName = "Critical"),
	Dead		UMETA(DisplayName = "Dead")
};

// ─────────────────────────────────────────────────────────
// ESubsystemStatus — per-subsystem health state
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ESubsystemStatus : uint8
{
	Operational		UMETA(DisplayName = "Operational"),
	Degraded		UMETA(DisplayName = "Degraded"),
	Destroyed		UMETA(DisplayName = "Destroyed")
};

// ─────────────────────────────────────────────────────────
// ESubsystemType — identifies which subsystem changed
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ESubsystemType : uint8
{
	Vision			UMETA(DisplayName = "Vision"),
	Audio			UMETA(DisplayName = "Audio"),
	Locomotion		UMETA(DisplayName = "Locomotion"),
	ManipulatorLeft	UMETA(DisplayName = "Manipulator Left"),
	ManipulatorRight UMETA(DisplayName = "Manipulator Right"),
	CommLink		UMETA(DisplayName = "Comm Link")
};

// ─────────────────────────────────────────────────────────
// EAndroidReadiness — overall operational readiness
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EAndroidReadiness : uint8
{
	FullyOperational	UMETA(DisplayName = "Fully Operational"),
	Degraded			UMETA(DisplayName = "Degraded"),
	NeedsMaintenance	UMETA(DisplayName = "Needs Maintenance"),
	Disabled			UMETA(DisplayName = "Disabled")
};

// ─────────────────────────────────────────────────────────
// EAbortReason — why the Foreman is aborting a task
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EAbortReason : uint8
{
	Reassigned		UMETA(DisplayName = "Reassigned"),
	TargetLost		UMETA(DisplayName = "Target Lost"),
	Emergency		UMETA(DisplayName = "Emergency"),
	ForemanOrder	UMETA(DisplayName = "Foreman Order")
};

// ─────────────────────────────────────────────────────────
// EWorkEndReason — how a work interaction concluded
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EWorkEndReason : uint8
{
	Completed	UMETA(DisplayName = "Completed"),
	Aborted		UMETA(DisplayName = "Aborted"),
	Failed		UMETA(DisplayName = "Failed")
};
