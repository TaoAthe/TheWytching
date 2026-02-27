// Example: How to Use Vision AI for Game Logic
// Add this to your game's AI controller or pawn

#pragma once

#include "CoreMinimal.h"

// Example structure to hold vision decisions
USTRUCT(BlueprintType)
struct FVisionDecision
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString Action;

    UPROPERTY(BlueprintReadWrite)
    FString Target;

    UPROPERTY(BlueprintReadWrite)
    FString Reasoning;
};

// Example: Action handler class
class FVisionActionHandler
{
public:
    static void ExecuteAction(const FVisionDecision& Decision, AActor* Actor)
    {
        if (Decision.Action == "pick_up")
        {
            HandlePickup(Decision.Target, Actor);
        }
        else if (Decision.Action == "approach" || Decision.Action == "move_to")
        {
            HandleApproach(Decision.Target, Actor);
        }
        else if (Decision.Action == "attack")
        {
            HandleAttack(Decision.Target, Actor);
        }
        else if (Decision.Action == "flee" || Decision.Action == "flee_from")
        {
            HandleFlee(Decision.Target, Actor);
        }
        else if (Decision.Action == "use" || Decision.Action == "activate")
        {
            HandleUse(Decision.Target, Actor);
        }
        else if (Decision.Action == "investigate")
        {
            HandleInvestigate(Decision.Target, Actor);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Unknown action: %s"), *Decision.Action);
        }
    }

private:
    static void HandlePickup(const FString& Target, AActor* Actor)
    {
        UE_LOG(LogTemp, Log, TEXT("AI wants to pick up: %s"), *Target);
        
        // Find nearest object matching description
        // Example: Search for actors with tag or name containing Target
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(
            Actor->GetWorld(), AActor::StaticClass(), FoundActors);
        
        for (AActor* FoundActor : FoundActors)
        {
            FString ActorName = FoundActor->GetName().ToLower();
            FString TargetLower = Target.ToLower();
            
            if (ActorName.Contains(TargetLower) || 
                FoundActor->Tags.Contains(FName(*Target)))
            {
                // Move to and pick up
                UE_LOG(LogTemp, Log, TEXT("Found target: %s"), *ActorName);
                // Add your pickup logic here
                break;
            }
        }
    }

    static void HandleApproach(const FString& Target, AActor* Actor)
    {
        UE_LOG(LogTemp, Log, TEXT("AI approaching: %s"), *Target);
        
        // Find target and move AI toward it
        // Use AI navigation system
        // Example with AI Controller:
        /*
        if (AAIController* AIController = Cast<AAIController>(Actor->GetInstigatorController()))
        {
            AActor* TargetActor = FindActorByDescription(Target, Actor->GetWorld());
            if (TargetActor)
            {
                AIController->MoveToActor(TargetActor);
            }
        }
        */
    }

    static void HandleAttack(const FString& Target, AActor* Actor)
    {
        UE_LOG(LogTemp, Log, TEXT("AI attacking: %s"), *Target);
        
        // Set AI state to combat
        // Target the specified object/enemy
        // Example:
        // SetAIState(EAIState::Combat);
        // SetTarget(FindActorByDescription(Target));
    }

    static void HandleFlee(const FString& Target, AActor* Actor)
    {
        UE_LOG(LogTemp, Log, TEXT("AI fleeing from: %s"), *Target);
        
        // Move away from target
        // Find safe location
        // Example:
        // FVector FleeDirection = Actor->GetActorLocation() - ThreatLocation;
        // AIController->MoveToLocation(Actor->GetActorLocation() + FleeDirection * 1000.f);
    }

    static void HandleUse(const FString& Target, AActor* Actor)
    {
        UE_LOG(LogTemp, Log, TEXT("AI using: %s"), *Target);
        
        // Interact with object
        // Trigger use/activation
        // Example: Press button, open door, etc.
    }

    static void HandleInvestigate(const FString& Target, AActor* Actor)
    {
        UE_LOG(LogTemp, Log, TEXT("AI investigating: %s"), *Target);
        
        // Move closer to examine
        // Change AI state to curious/alert
    }

    // Helper function to find actor by description
    static AActor* FindActorByDescription(const FString& Description, UWorld* World)
    {
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
        
        FString DescLower = Description.ToLower();
        
        for (AActor* Actor : AllActors)
        {
            FString ActorName = Actor->GetName().ToLower();
            
            // Check name contains description
            if (ActorName.Contains(DescLower))
            {
                return Actor;
            }
            
            // Check tags
            for (const FName& Tag : Actor->Tags)
            {
                if (Tag.ToString().ToLower().Contains(DescLower))
                {
                    return Actor;
                }
            }
            
            // Check for color keywords
            if (Description.Contains("blue") && Actor->Tags.Contains("Blue"))
            {
                return Actor;
            }
            // Add more color/attribute checks as needed
        }
        
        return nullptr;
    }
};

// Example: Integration in OllamaDebugActor
// Add this to OnResponseReceived after parsing the JSON:

/*
void AOllamaDebugActor::OnResponseReceived(...)
{
    // ...existing parsing code...
    
    if (FJsonSerializer::Deserialize(ContentReader, ContentJson))
    {
        FVisionDecision Decision;
        Decision.Action = ContentJson->GetStringField(TEXT("action"));
        Decision.Target = ContentJson->GetStringField(TEXT("target"));
        Decision.Reasoning = ContentJson->GetStringField(TEXT("reasoning"));
        
        // Display info (existing code)
        // ...
        
        // NEW: Execute the action!
        FVisionActionHandler::ExecuteAction(Decision, this);
    }
}
*/

// Example: Blueprint-exposed function
// Add to your AI Character or Controller

UCLASS()
class UVisionAIComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Vision AI")
    void ProcessVisionDecision(const FString& Action, const FString& Target, const FString& Reasoning)
    {
        FVisionDecision Decision;
        Decision.Action = Action;
        Decision.Target = Target;
        Decision.Reasoning = Reasoning;
        
        AActor* Owner = GetOwner();
        FVisionActionHandler::ExecuteAction(Decision, Owner);
        
        // Broadcast event for Blueprint handling
        OnVisionDecisionMade.Broadcast(Decision);
    }

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVisionDecision, FVisionDecision, Decision);
    
    UPROPERTY(BlueprintAssignable, Category = "Vision AI")
    FOnVisionDecision OnVisionDecisionMade;
};

