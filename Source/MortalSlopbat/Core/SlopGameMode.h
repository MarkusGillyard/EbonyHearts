#pragma once

#include "GameFramework/GameModeBase.h"
#include "SlopGameMode.generated.h"

UCLASS()
class ASlopGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ASlopGameMode();
	virtual void BeginPlay() override;
};
