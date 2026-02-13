#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SailingGameInstance.generated.h"

UCLASS()
class SAILING_API USailingGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** Når true: MainOcean skal starte med ny lagringsfil (Nytt spill). */
	UPROPERTY(BlueprintReadWrite, Category = "Menu")
	bool bRequestNewGame = false;
};
