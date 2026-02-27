#include "PatrolPoint.h"
#include "Components/BillboardComponent.h"

APatrolPoint::APatrolPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(SceneRoot);
	Billboard->bIsScreenSizeScaled = true;

#if WITH_EDITORONLY_DATA
	Billboard->SetSprite(LoadObject<UTexture2D>(nullptr,
		TEXT("/Engine/EditorResources/S_Note.S_Note")));
#endif
}
