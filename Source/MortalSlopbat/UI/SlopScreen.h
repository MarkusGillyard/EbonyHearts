#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Core/SlopInputRouter.h"

class USlopGameInstance;

// One screen of the game (a menu, the pause overlay, results...). It builds its Slate widgets
// and reacts to menu actions from the input router; it never relies on Slate keyboard focus.
class FSlopScreen : public TSharedFromThis<FSlopScreen>
{
public:
	virtual ~FSlopScreen() = default;
	virtual TSharedRef<SWidget> Build() = 0;
	virtual void OnUi(const Slop::FUiEvent& E) {}
	virtual void Tick(float Dt) {}
	// Called when the screen is shown (after Build) and when it is replaced.
	virtual void Enter() {}
	virtual void Leave() {}
	// Whether a running match keeps drawing behind this screen.
	virtual bool ShowsMatch() const { return false; }
	USlopGameInstance* GI = nullptr;

protected:
	// Moves a selection index through a list with up/down (wrapping), skipping disabled items.
	static int32 Step(int32 Index, int32 Count, int32 Delta, TFunction<bool(int32)> Enabled = nullptr);
	void Sound(const TCHAR* Name);
};
