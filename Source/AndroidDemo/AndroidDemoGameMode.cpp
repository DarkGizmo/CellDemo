// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidDemoGameMode.h"
#include "AndroidDemoPlayerController.h"
#include "AndroidDemoCharacter.h"
#include "UObject/ConstructorHelpers.h"

AAndroidDemoGameMode::AAndroidDemoGameMode()
{
	// use our custom PlayerController class
	PlayerControllerClass = AAndroidDemoPlayerController::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/TopDownCPP/Blueprints/TopDownCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}