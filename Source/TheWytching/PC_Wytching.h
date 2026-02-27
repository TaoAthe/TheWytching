// PC_Wytching.h — Player Controller for TheWytching
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PC_Wytching.generated.h"

UCLASS()
class THEWYTCHING_API APC_Wytching : public APlayerController
{
	GENERATED_BODY()

public:
	APC_Wytching();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void CommandForemanSearch();
	void PossessDrone();
};
