#include "Core/SlopGameMode.h"
#include "Core/SlopLog.h"

ASlopGameMode::ASlopGameMode()
{
	DefaultPawnClass = nullptr;
}

void ASlopGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSlop, Log, TEXT("Mortal Slopbat game mode started"));
}
