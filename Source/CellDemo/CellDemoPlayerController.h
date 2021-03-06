// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CellDemoPlayerController.generated.h"

UCLASS()
class ACellDemoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACellDemoPlayerController();

	UPROPERTY(BlueprintReadOnly)
	FName OnlineSessionName;

	UPROPERTY(BlueprintReadOnly)
	FString OnlineSessionId;

	UPROPERTY(BlueprintReadOnly)
	bool Connecting;

	UFUNCTION(BlueprintImplementableEvent, Category = "Network")
	void OnConnecting();

	UFUNCTION(BlueprintImplementableEvent, Category = "Network")
	void OnConnected();

	UFUNCTION(BlueprintImplementableEvent, Category = "Network")
	void OnDisconnected();
	
protected:
	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	// Begin PlayerController interface
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	// End PlayerController interface

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Navigate player to the current mouse cursor location. */
	void MoveToMouseCursor();

	/** Navigate player to the current touch location. */
	void MoveToTouchLocation(const ETouchIndex::Type FingerIndex, const FVector Location);
	
	/** Navigate player to the given world location. */
	UFUNCTION(Server, Reliable, WithValidation)
	void SetNewMoveDestination(const FVector DestLocation);

	/** Input handlers for SetDestination action. */
	void OnSetDestinationPressed();
	void OnSetDestinationReleased();

	virtual void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());
};


