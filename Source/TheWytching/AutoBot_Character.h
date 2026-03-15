#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "IWytchCommandable.h"
#include "AndroidTypes.h"
#include "SmartObjectRuntime.h"
#include "AutoBot_Character.generated.h"

class UAndroidConditionComponent;

// ─────────────────────────────────────────────────────────
// EAutoBot_Class — three physical tiers (DEC-006)
//   Scout   : ~1m  — fast, light, Task.Scout / Task.Recon
//   Light   : ~2m  — default worker, Task.Build / Task.Transport
//   Heavy   : ~6m  — slow, strong, Task.Build / Task.Demolition / Task.Carry.Heavy
//
// Class is set per Blueprint child and drives capsule/mesh scale
// via the AutoBotClass property. One skeleton (SK_Mech), three scale values.
// ─────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EAutoBot_Class : uint8
{
	Scout	UMETA(DisplayName = "Scout (1m)"),
	Light	UMETA(DisplayName = "Light Worker (2m)"),
	Heavy	UMETA(DisplayName = "Heavy Worker (6m)")
};

/**
 * AAutoBot_Character
 *
 * Specialist worker android. Implements IWytchCommandable so the Foreman can
 * assign SmartObject slots, query state, and abort tasks.
 *
 * Parent: ACharacter — CharacterMovementComponent + CapsuleComponent + SkeletalMesh
 * already wired. Animation handled by CR_Mech Control Rig (existing asset).
 *
 * Three Blueprint children, one C++ parent:
 *   BP_AutoBot_Scout   (EAutoBot_Class::Scout, scale ~0.5)
 *   BP_AutoBot_Light   (EAutoBot_Class::Light, scale ~1.0)
 *   BP_AutoBot_Heavy   (EAutoBot_Class::Heavy, scale ~3.0)
 *
 * Refs: DEC-003, DEC-004, DEC-005, DEC-006
 */
UCLASS(Blueprintable, BlueprintType, Abstract)
class THEWYTCHING_API AAutoBot_Character : public ACharacter, public IWytchCommandable
{
	GENERATED_BODY()

public:
	AAutoBot_Character();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── IWytchCommandable ─────────────────────────────────
	// BlueprintNativeEvent — BPs may override; C++ _Implementation is the default.

	virtual void ReceiveSmartObjectAssignment_Implementation(
		FSmartObjectClaimHandle ClaimHandle,
		FSmartObjectSlotHandle SlotHandle,
		AActor* TargetActor) override;

	virtual EWorkerState GetWorkerState_Implementation() const override;

	virtual FGameplayTagContainer GetCapabilities_Implementation() const override;

	virtual void AbortCurrentTask_Implementation(EAbortReason Reason) override;

	// ── Accessors ─────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "AutoBot|State")
	EWorkerState GetCurrentWorkerState() const { return WorkerState; }

	UFUNCTION(BlueprintCallable, Category = "AutoBot|Condition")
	UAndroidConditionComponent* GetCondition() const { return ConditionComponent; }

	UFUNCTION(BlueprintCallable, Category = "AutoBot|State")
	bool HasActiveAssignment() const { return PendingSlotHandle.IsValid(); }

	// ── Designer vars (DEC-005 — BP-editable, logic in C++) ──

	/**
	 * Physical class of this AutoBot. Set once per Blueprint child — do not change at runtime.
	 * Drives mesh scale in BP defaults (not in C++ — BP sets RootComponent->SetRelativeScale3D).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AutoBot|Class")
	EAutoBot_Class AutoBotClass = EAutoBot_Class::Light;

	/**
	 * Capability tags for this AutoBot class. Pre-populated in each BP child:
	 *   Scout: Task.Scout, Task.Recon
	 *   Light: Task.Build, Task.Transport
	 *   Heavy: Task.Build, Task.Demolition, Task.Carry.Heavy
	 * Merged into AndroidConditionComponent.BaseCapabilities at BeginPlay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBot|Capabilities")
	FGameplayTagContainer SpecialistCapabilities;

	/** How close to the slot origin before "arrived" triggers and claim begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBot|Movement",
		meta = (ClampMin = "10.0", ClampMax = "300.0"))
	float ArrivalAcceptanceRadius = 80.f;

	/** Arrival check poll rate (seconds). Lower = more responsive, higher = cheaper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBot|Movement",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ArrivalCheckInterval = 0.25f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoBot|Condition")
	TObjectPtr<UAndroidConditionComponent> ConditionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoBot|State")
	EWorkerState WorkerState = EWorkerState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoBot|State")
	FSmartObjectSlotHandle PendingSlotHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoBot|State")
	FSmartObjectClaimHandle ActiveClaimHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoBot|State")
	TObjectPtr<AActor> TargetWorkActor = nullptr;

private:
	void BeginNavigationToSlot();
	void OnArrivedAtSlot();
	void ReleaseAndReturnToIdle();
	void RegisterWithForeman();

	FTimerHandle ArrivalCheckTimerHandle;
	FVector SlotDestination = FVector::ZeroVector;
};
