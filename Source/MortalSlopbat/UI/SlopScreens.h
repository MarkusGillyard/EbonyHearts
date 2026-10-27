#pragma once

#include "UI/SlopScreen.h"
#include "Core/SlopFlow.h"

// Factories for every screen, so the flow between them reads in one place.
namespace SlopScreens
{
	TSharedPtr<FSlopScreen> Title();
	TSharedPtr<FSlopScreen> MainMenu();
	TSharedPtr<FSlopScreen> MatchScreen();
	TSharedPtr<FSlopScreen> Pause();
	TSharedPtr<FSlopScreen> Results(const Slop::FMatchResult& R);
	TSharedPtr<FSlopScreen> DataError();
	// Fighter select for a mode; Versus asks both sides.
	TSharedPtr<FSlopScreen> Select(Slop::EMode Mode);
	TSharedPtr<FSlopScreen> Stage();
	TSharedPtr<FSlopScreen> Tower(FName Fighter);
	TSharedPtr<FSlopScreen> FaceOff();
	TSharedPtr<FSlopScreen> Devices();
	TSharedPtr<FSlopScreen> Settings(bool bFromPause);
	TSharedPtr<FSlopScreen> MoveList(FName Fighter, bool bOverlay);
	TSharedPtr<FSlopScreen> Extras();
	TSharedPtr<FSlopScreen> Ending(FName Fighter, bool bFromArcade);
	TSharedPtr<FSlopScreen> StrangerOffer();
	TSharedPtr<FSlopScreen> Credits();
	TSharedPtr<FSlopScreen> TrialsPick();
}
