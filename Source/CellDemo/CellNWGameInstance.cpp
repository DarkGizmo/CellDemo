// Fill out your copyright notice in the Description page of Project Settings.

#include "CellNWGameInstance.h"

#include "Engine.h"
#include "Online.h"

UCellNWGameInstance::UCellNWGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	/** Bind function for CREATING a Session */
	OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCellNWGameInstance::OnCreateSessionComplete);
	OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &UCellNWGameInstance::OnStartOnlineGameComplete);

	/** Bind function for FINDING a Session */
	OnFindAndJoinFindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCellNWGameInstance::OnFindAndJoinFindSessionsComplete);

	/** Bind function for JOINING a Session */
	OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCellNWGameInstance::OnJoinSessionComplete);

	/** Bind function for DESTROYING a Session */
	OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCellNWGameInstance::OnDestroySessionComplete);

	bShowDebugMsg = false;
}

// *******************************
// Hosting
// *******************************

bool UCellNWGameInstance::HostSession(TSharedPtr<const FUniqueNetId> UserId, FString MapName, FString SessionId, FName SessionName, bool bIsLAN, bool bIsPresence, int32 MaxNumPlayers)
{
	// Get the Online Subsystem to work with
	IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get();

	if (OnlineSub)
	{
		// Get the Session Interface, so we can call the "CreateSession" function on it
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid() && UserId.IsValid())
		{
			/*
			Fill in all the Session Settings that we want to use.

			There are more with SessionSettings.Set(...);
			For example the Map or the GameMode/Type.
			*/
			SessionSettings = MakeShareable(new FOnlineSessionSettings());

			SessionSettings->bIsLANMatch = bIsLAN;
			SessionSettings->bUsesPresence = bIsPresence;
			SessionSettings->NumPublicConnections = MaxNumPlayers;
			SessionSettings->NumPrivateConnections = 0;
			SessionSettings->bAllowInvites = true;
			SessionSettings->bAllowJoinInProgress = true;
			SessionSettings->bShouldAdvertise = true;
			SessionSettings->bAllowJoinViaPresence = true;
			SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;

			SessionSettings->Set(SETTING_MAPNAME, MapName, EOnlineDataAdvertisementType::ViaOnlineService);
			SessionSettings->Set(FName(TEXT("SessionId")), SessionId, EOnlineDataAdvertisementType::ViaOnlineService);

			// Set the delegate to the Handle of the SessionInterface
			OnCreateSessionCompleteDelegateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);

			if (bShowDebugMsg)
			{
				GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::Red, FString::Printf(TEXT("Creating Session with SessionId: %s "), *SessionId));
			}

			// Our delegate should get called when this is complete (doesn't need to be successful!)
			return Sessions->CreateSession(*UserId, SessionName, *SessionSettings);
		}
	}
	else
	{
		if (bShowDebugMsg)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("No OnlineSubsytem found!"));
		}
	}

	return false;
}

void UCellNWGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bShowDebugMsg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnCreateSessionComplete %s, %d"), *SessionName.ToString(), bWasSuccessful));
	}

	// Get the OnlineSubsystem so we can get the Session Interface
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get the Session Interface to call the StartSession function
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			// Clear the SessionComplete delegate handle, since we finished this call
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
			if (bWasSuccessful)
			{
				// Set the StartSession delegate handle
				OnStartSessionCompleteDelegateHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);

				// Our StartSessionComplete delegate should get called after this
				Sessions->StartSession(SessionName);
			}
		}

	}
}

void UCellNWGameInstance::OnStartOnlineGameComplete(FName SessionName, bool bWasSuccessful)
{
	if (bShowDebugMsg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnStartSessionComplete %s, %d"), *SessionName.ToString(), bWasSuccessful));
	}

	// Get the Online Subsystem so we can get the Session Interface
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get the Session Interface to clear the Delegate
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			// Clear the delegate, since we are done with this call
			Sessions->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
		}

		if (bWasSuccessful)
		{
			APlayerController * const PlayerController = GetFirstLocalPlayerController();
			ACellDemoPlayerController* cellDemoPlayerController = Cast<ACellDemoPlayerController>(PlayerController);
			if (cellDemoPlayerController != nullptr)
			{
				FNamedOnlineSession* namedSession = Sessions.Get()->GetNamedSession(SessionName);
				if (namedSession != nullptr)
				{
					FString sessionId;
					if (namedSession->SessionSettings.Get(FName(TEXT("SessionId")), sessionId))
					{
						cellDemoPlayerController->OnlineSessionName = SessionName;
						cellDemoPlayerController->OnlineSessionId = sessionId;
						CurrentSessionId = sessionId;
					}

					FString mapName;
					if (namedSession->SessionSettings.Get(SETTING_MAPNAME, mapName))
					{
						UGameplayStatics::OpenLevel(GetWorld(), FName(*mapName), true, "listen");
					}
				}
			}
		}
	}
}

// *******************************
// Finding
// *******************************

void UCellNWGameInstance::FindSessions(ULocalPlayer* const Player, bool bIsLAN, bool bIsPresence, bool bAutoJoin, const FString& SessionId)
{
	TSharedPtr<const FUniqueNetId> UserId = Player->GetPreferredUniqueNetId();

	if (bShowDebugMsg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("FindSessions Started")));
	}
	// Get the OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();

	if (OnlineSub)
	{
		// Get the SessionInterface from our OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid() && UserId.IsValid())
		{
			/*
			Fill in all the SearchSettings, like if we are searching for a LAN game and how many results we want to have!
			*/
			SessionSearch = MakeShareable(new FOnlineSessionSearch());

			SessionSearch->bIsLanQuery = bIsLAN;
			SessionSearch->MaxSearchResults = 20;
			SessionSearch->PingBucketSize = 50;

			// We only want to set this Query Setting if "bIsPresence" is true
			if (bIsPresence)
			{
				SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, bIsPresence, EOnlineComparisonOp::Equals);
			}

			TSharedRef<FOnlineSessionSearch> SearchSettingsRef = SessionSearch.ToSharedRef();

			// Set the Delegate to the Delegate Handle of the FindSession function
			if (bAutoJoin)
			{
				OnFindSessionsCompleteDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(OnFindAndJoinFindSessionsCompleteDelegate);
				
				ACellDemoPlayerController* controller = Cast<ACellDemoPlayerController>(Player->GetPlayerController(GetWorld()));
				if (controller != nullptr)
				{
					controller->Connecting = true;
					controller->OnlineSessionId = SessionId;
					controller->OnConnecting();
				}
			}

			// Finally call the SessionInterface function. The Delegate gets called once this is finished
			Sessions->FindSessions(*UserId, SearchSettingsRef);

			
		}
	}
	else
	{
		// If something goes wrong, just call the Delegate Function directly with "false".
		OnFindAndJoinFindSessionsComplete(false);
	}
}

void UCellNWGameInstance::OnFindAndJoinFindSessionsComplete(bool bWasSuccessful)
{
	if (bShowDebugMsg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OFindSessionsComplete bSuccess: %d"), bWasSuccessful));
	}

	ULocalPlayer* const Player = GetFirstGamePlayer();
	ACellDemoPlayerController* controller = Cast<ACellDemoPlayerController>(Player->GetPlayerController(GetWorld()));
	if (controller != nullptr)
	{
		controller->Connecting = false;
	}

	// Get OnlineSubsystem we want to work with
	IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get SessionInterface of the OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			// Clear the Delegate handle, since we finished this call
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);

			// Just debugging the Number of Search results. Can be displayed in UMG or something later on
			if (bShowDebugMsg)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Num Search Results: %d"), SessionSearch->SearchResults.Num()));
			}

			// If we have found at least 1 session, we just going to debug them. You could add them to a list of UMG Widgets, like it is done in the BP version!
			if (SessionSearch->SearchResults.Num() > 0)
			{

				// "SessionSearch->SearchResults" is an Array that contains all the information. You can access the Session in this and get a lot of information.
				// This can be customized later on with your own classes to add more information that can be set and displayed
				for (int32 SearchIdx = 0; SearchIdx < SessionSearch->SearchResults.Num(); SearchIdx++)
				{
					// OwningUserName is just the SessionName for now. I guess you can create your own Host Settings class and GameSession Class and add a proper GameServer Name here.
					// This is something you can't do in Blueprint for example!

					// To avoid something crazy, we filter sessions from ourself
					if (SessionSearch->SearchResults[SearchIdx].Session.OwningUserId != Player->GetPreferredUniqueNetId())
					{
						if (bShowDebugMsg)
						{
							GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Looking for Session with id %s "), *controller->OnlineSessionId));
						}

						FString sessionId;
						if (SessionSearch->SearchResults[SearchIdx].Session.SessionSettings.Get(FName(TEXT("SessionId")), sessionId))
						{
							if (bShowDebugMsg)
							{
								GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Session Number: %d | SessionId: %s "), SearchIdx + 1, *sessionId));
							}

							if (sessionId == controller->OnlineSessionId)
							{
								// Once we found sounce a Session that is not ours, just join it. Instead of using a for loop, you could
								// use a widget where you click on and have a reference for the GameSession it represents which you can use
								// here
								JoinOnlineSession(Player->GetPreferredUniqueNetId(), GameSessionName, SessionSearch->SearchResults[SearchIdx]);

								ULocalPlayer* const Player = GetFirstGamePlayer();
								ACellDemoPlayerController* controller = Cast<ACellDemoPlayerController>(Player->GetPlayerController(GetWorld()));
								if (controller != nullptr)
								{
									controller->Connecting = true;
								}
							}

							break;
						}
					}
				}
			}
		}
	}
}

// *******************************
// Joining
// *******************************

bool UCellNWGameInstance::JoinOnlineSession(TSharedPtr<const FUniqueNetId> UserId, FName SessionName, const FOnlineSessionSearchResult& SearchResult)
{
	// Return bool
	bool bSuccessful = false;

	ULocalPlayer* const Player = GetFirstGamePlayer();
	ACellDemoPlayerController* controller = Cast<ACellDemoPlayerController>(Player->GetPlayerController(GetWorld()));
	if (controller != nullptr)
	{
		controller->Connecting = false;
	}

	// Get OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();

	if (OnlineSub)
	{
		// Get SessionInterface from the OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid() && UserId.IsValid())
		{
			// Set the Handle again
			OnJoinSessionCompleteDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);

			// Call the "JoinOnlineSession" Function with the passed "SearchResult". The "SessionSearch->SearchResults" can be used to get such a
			// "FOnlineSessionSearchResult" and pass it. Pretty straight forward!
			bSuccessful = Sessions->JoinSession(*UserId, SessionName, SearchResult);

			ULocalPlayer* const Player = GetFirstGamePlayer();
			ACellDemoPlayerController* controller = Cast<ACellDemoPlayerController>(Player->GetPlayerController(GetWorld()));
			if (controller != nullptr)
			{
				controller->Connecting = true;
			}
		}
	}

	return bSuccessful;
}

void UCellNWGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (bShowDebugMsg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnJoinSessionComplete %s, %d"), *SessionName.ToString(), static_cast<int32>(Result)));
	}

	ULocalPlayer* const Player = GetFirstGamePlayer();
	ACellDemoPlayerController* controller = Cast<ACellDemoPlayerController>(Player->GetPlayerController(GetWorld()));
	if (controller != nullptr)
	{
		controller->Connecting = false;
	}

	// Get the OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get SessionInterface from the OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			// Clear the Delegate again
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);

			// Get the first local PlayerController, so we can call "ClientTravel" to get to the Server Map
			// This is something the Blueprint Node "Join Session" does automatically!
			APlayerController * const PlayerController = GetFirstLocalPlayerController();

			// We need a FString to use ClientTravel and we can let the SessionInterface contruct such a
			// String for us by giving him the SessionName and an empty String. We want to do this, because
			// Every OnlineSubsystem uses different TravelURLs
			FString TravelURL;

			if (PlayerController && Sessions->GetResolvedConnectString(SessionName, TravelURL))
			{
				// Finally call the ClienTravel. If you want, you could print the TravelURL to see
				// how it really looks like
				PlayerController->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
			}

			ACellDemoPlayerController* cellDemoPlayerController = Cast<ACellDemoPlayerController>(PlayerController);
			if (cellDemoPlayerController != nullptr)
			{
				FString sessionId;
				FNamedOnlineSession* namedSession = Sessions.Get()->GetNamedSession(SessionName);
				if (namedSession != nullptr && namedSession->SessionSettings.Get(FName(TEXT("SessionId")), sessionId))
				{
					cellDemoPlayerController->OnlineSessionName = SessionName;
					cellDemoPlayerController->OnlineSessionId = sessionId;

					cellDemoPlayerController->OnConnected();
				}
			}
		}
	}
}

// *******************************
// Destroying
// *******************************


void UCellNWGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bShowDebugMsg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnDestroySessionComplete %s, %d"), *SessionName.ToString(), bWasSuccessful));
	}

	// Get the OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get the SessionInterface from the OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			// Clear the Delegate
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);

			APlayerController * const PlayerController = GetFirstLocalPlayerController();
			ACellDemoPlayerController* cellDemoPlayerController = Cast<ACellDemoPlayerController>(PlayerController);
			if (cellDemoPlayerController != nullptr)
			{
				cellDemoPlayerController->OnlineSessionName = NAME_None;
				cellDemoPlayerController->OnlineSessionId = FString("");
				CurrentSessionId = FString("");
				cellDemoPlayerController->OnDisconnected();
			}

			// If it was successful, we just load another level (could be a MainMenu!)
			if (bWasSuccessful)
			{
				UGameplayStatics::OpenLevel(GetWorld(), "Phone", true);
			}
		}
	}
}

// *******************************
// Blueprint
// *******************************


void UCellNWGameInstance::StartOnlineGame(FString MapName, int NumberOfPlayer, FString SessionId)
{
	// Creating a local player where we can get the UserID from
	ULocalPlayer* const Player = GetFirstGamePlayer();

	// Call our custom HostSession function. GameSessionName is a GameInstance variable
	HostSession(Player->GetPreferredUniqueNetId(), MapName, SessionId, GameSessionName, true, true, NumberOfPlayer);
}

void UCellNWGameInstance::FindOnlineGames(FString SessionId)
{
	ULocalPlayer* const Player = GetFirstGamePlayer();

	FindSessions(Player, true, true, false, SessionId);
}

void UCellNWGameInstance::FindAndJoinOnlineGame(FString SessionId)
{
	ULocalPlayer* const Player = GetFirstGamePlayer();

	FindSessions(Player, true, true, true, SessionId);
}

void UCellNWGameInstance::JoinOnlineGame()
{
	ULocalPlayer* const Player = GetFirstGamePlayer();

	// Just a SearchResult where we can save the one we want to use, for the case we find more than one!
	FOnlineSessionSearchResult SearchResult;

	// If the Array is not empty, we can go through it
	if (SessionSearch->SearchResults.Num() > 0)
	{
		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
		{
			// To avoid something crazy, we filter sessions from ourself
			if (SessionSearch->SearchResults[i].Session.OwningUserId != Player->GetPreferredUniqueNetId())
			{
				SearchResult = SessionSearch->SearchResults[i];

				// Once we found sounce a Session that is not ours, just join it. Instead of using a for loop, you could
				// use a widget where you click on and have a reference for the GameSession it represents which you can use
				// here
				JoinOnlineSession(Player->GetPreferredUniqueNetId(), GameSessionName, SearchResult);
				break;
			}
		}
	}
}

void UCellNWGameInstance::DestroySessionAndLeaveGame()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			Sessions->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);

			Sessions->DestroySession(GameSessionName);
		}
	}
}

void UCellNWGameInstance::GetOnlineGameStatus(ACellDemoPlayerController* controller, bool& bIsInOnlineGame, bool& bIsServer, FString& SessionName)
{
	if (controller == nullptr)
	{
		return;
	}
	bIsInOnlineGame = false;
	bIsServer = false;
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			AGameStateBase* gameState = UGameplayStatics::GetGameState(this);
			
			if (gameState != nullptr)
			{
				if (gameState->PlayerArray.Num() > 1)
				{
					bIsInOnlineGame = true;
					bIsServer = gameState->HasAuthority();
				}
			}
		}
	}
}