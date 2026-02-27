#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Foreman_Character.generated.h"

UCLASS()
class THEWYTCHING_API AForeman_Character : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AForeman_Character();
	virtual void BeginPlay() override;

	// IAbilitySystemInterface implementation
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS", meta=(AllowPrivateAccess="true"))
	class UAbilitySystemComponent* AbilitySystemComponent;
};
