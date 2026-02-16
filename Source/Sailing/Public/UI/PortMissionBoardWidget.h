#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/SailingProgressionTypes.h"
#include "PortMissionBoardWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortMissionAcceptRequested, FName, MissionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortMissionBoardCloseRequested);

USTRUCT(BlueprintType)
struct SAILING_API FPortMissionOfferEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText MissionTitle;
};

USTRUCT(BlueprintType)
struct SAILING_API FPortMissionBoardData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName PortId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText PortDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	TArray<FName> OfferedMissionIds;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	TArray<FPortMissionOfferEntry> OfferedMissions;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName CurrentMissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	bool bMissionBoardOnCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	float CooldownRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	TArray<FMissionBoardSelectionEntry> RecentSelections;
};

/**
 * Optional UMG widget hook for a lightweight mission board at ports.
 */
UCLASS(BlueprintType, Blueprintable)
class SAILING_API UPortMissionBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MissionBoard")
	void PushMissionBoardData(const FPortMissionBoardData& InData);

	UFUNCTION(BlueprintCallable, Category = "MissionBoard")
	void RequestAcceptMission(FName MissionId);

	UFUNCTION(BlueprintCallable, Category = "MissionBoard")
	void RequestCloseBoard();

	UFUNCTION(BlueprintPure, Category = "MissionBoard")
	const FPortMissionBoardData& GetLastData() const { return LastData; }

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard")
	FOnPortMissionAcceptRequested OnAcceptMissionRequested;

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard")
	FOnPortMissionBoardCloseRequested OnCloseRequested;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "MissionBoard")
	void OnMissionBoardDataUpdated(const FPortMissionBoardData& InData);

private:
	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard", meta = (AllowPrivateAccess = "true"))
	FPortMissionBoardData LastData;
};
