#pragma once

#include "CoreMinimal.h"
#include "SailingProgressionTypes.generated.h"

USTRUCT(BlueprintType)
struct SAILING_API FMissionBoardSelectionEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MissionBoard")
	FName PortId = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "MissionBoard")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "MissionBoard")
	FDateTime AcceptedTime;
};
