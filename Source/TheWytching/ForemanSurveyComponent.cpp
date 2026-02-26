#include "ForemanSurveyComponent.h"
#include "PatrolPoint.h"
#include "ForemanTypes.h"
#include "IWytchInteractable.h"
#include "Kismet/GameplayStatics.h"

UForemanSurveyComponent::UForemanSurveyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APatrolPoint* UForemanSurveyComponent::GetNextPatrolPoint()
{
	if (PatrolPoints.IsEmpty()) return nullptr;
    
	APatrolPoint* Next = PatrolPoints[CurrentPatrolIndex];
	CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();
	return Next;
}

int32 UForemanSurveyComponent::CountNearbyWorkers(FVector Origin)
{
	int32 Count = 0;
	TArray<AActor*> Workers;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Worker"), Workers);

	for (AActor* Worker : Workers)
	{
		if (FVector::Dist(Worker->GetActorLocation(), Origin) <= SurveyRadius)
		{
			Count++;
		}
	}
	return Count;
}

FString UForemanSurveyComponent::GenerateStatusReport()
{
	TArray<AActor*> AllWorkers;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Worker"), AllWorkers);

	int32 TotalWorkers = AllWorkers.Num();
	int32 IdleWorkers = 0;

	for (AActor* Worker : AllWorkers)
	{
		if (Worker->ActorHasTag(FName("Idle")))
		{
			IdleWorkers++;
		}
	}

	return FString::Printf(
		TEXT("Foreman Report: %d total workers, %d idle, %d active"),
		TotalWorkers,
		IdleWorkers,
		TotalWorkers - IdleWorkers);
}

TArray<AActor*> UForemanSurveyComponent::DiscoverInteractables(float SearchRadius)
{
	TArray<AActor*> Result;
	AActor* Owner = GetOwner();
	if (!Owner) return Result;

	const FVector Origin = Owner->GetActorLocation();
	const float RadiusSq = SearchRadius * SearchRadius;

	TArray<AActor*> All;
	UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UWytchInteractable::StaticClass(), All);

	for (AActor* Actor : All)
	{
		if (Actor && FVector::DistSquared(Actor->GetActorLocation(), Origin) <= RadiusSq)
		{
			Result.Add(Actor);
		}
	}

	UE_LOG(LogForeman, Log, TEXT("DiscoverInteractables: Found %d within %.0f units"), Result.Num(), SearchRadius);
	return Result;
}
