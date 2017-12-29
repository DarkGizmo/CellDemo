// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CellDemoGameMode.h"
#include "CellDemoPlayerController.h"
#include "CellDemoCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACellDemoGameMode::ACellDemoGameMode()
{
	// use our custom PlayerController class
	PlayerControllerClass = ACellDemoPlayerController::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/TopDownCPP/Blueprints/TopDownCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}