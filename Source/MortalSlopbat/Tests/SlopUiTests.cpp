#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "Core/SlopInputRouter.h"
#include "Core/SlopSave.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/SlopGmHarness.h"
#include "UI/SlopHud.h"
#include "UI/SlopStyle.h"

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace SlopUiTestSuite
{
using namespace Slop;
using namespace Slop::Test;

// --- The canvas ------------------------------------------------------------------------------------

SLOP_TEST(FSlopUiCanvas, "Ui.CanvasHoldsTheDesignOnEveryScreen")
bool FSlopUiCanvas::RunTest(const FString&)
{
	const FVector2D Shapes[] = {{1920, 1080}, {2560, 1440}, {3440, 1440}, {1920, 1200}, {1440, 1080}, {1280, 720}, {3840, 2160}, {800, 600}};
	for (const FVector2D& S : Shapes)
	{
		const FString Name = FString::Printf(TEXT("%.0fx%.0f"), S.X, S.Y);
		const FVector2D C = SlopUI::StageSize(S);
		TestTrue(Name + TEXT(": the 1920 x 1080 design always fits"), C.X >= 1919.9 && C.Y >= 1079.9);
		TestTrue(Name + TEXT(": one side is exactly the design's"), FMath::IsNearlyEqual(C.X, 1920.0, 0.1) || FMath::IsNearlyEqual(C.Y, 1080.0, 0.1));
		TestTrue(Name + TEXT(": nothing is stretched"), FMath::IsNearlyEqual(C.X / C.Y, S.X / S.Y, 0.001));
		TestTrue(Name + TEXT(": the canvas fills the screen"), FMath::IsNearlyEqual(C.X * SlopUI::StageScale(S), S.X, 0.5));
	}
	TestEqual("a wide screen widens the canvas", FMath::RoundToInt(SlopUI::StageSize(FVector2D(3440, 1440)).X), 2580);
	TestEqual("a 4:3 screen heightens it", FMath::RoundToInt(SlopUI::StageSize(FVector2D(1440, 1080)).Y), 1440);
	return true;
}

// --- Assets ----------------------------------------------------------------------------------------

SLOP_TEST(FSlopUiCard, "Ui.TitleCardIsTheOwnersOwn")
bool FSlopUiCard::RunTest(const FString&)
{
	// The source is byte for byte what he supplied, and the imported card is its full size at 3:1.
	const FString Source = FPaths::ProjectDir() / TEXT("SourceArt/UI/EbonyHearts_TitleCard.png");
	TestEqual("the supplied file", IFileManager::Get().FileSize(*Source), (int64)787347);
	const UTexture2D* T = Cast<UTexture2D>(SlopUI::Brush(SlopUI::ETex::Logo)->GetResourceObject());
	if (!TestNotNull("imported", T))
	{
		return false;
	}
#if WITH_EDITORONLY_DATA
	// The source pixels: a headless run has no platform texture to ask.
	TestEqual("its width", (int32)T->Source.GetSizeX(), 2172);
	TestEqual("its height", (int32)T->Source.GetSizeY(), 724);
#endif
	const FVector2D Size = SlopUI::Brush(SlopUI::ETex::Logo)->ImageSize;
	TestTrue("drawn at its own shape", FMath::IsNearlyEqual(Size.X / Size.Y, 3.0, 0.001));
	return true;
}

SLOP_TEST(FSlopUiTextures, "Ui.EveryInterfaceTextureIsThere")
bool FSlopUiTextures::RunTest(const FString&)
{
	for (int32 I = 0; I < (int32)SlopUI::ETex::Count; ++I)
	{
		const FSlateBrush* B = SlopUI::Brush((SlopUI::ETex)I);
		TestNotNull(FString::Printf(TEXT("texture %d"), I), B->GetResourceObject());
	}
	for (const FName& Id : FContent::Roster())
	{
		TestNotNull(FString::Printf(TEXT("portrait of %s"), *Id.ToString()), SlopUI::Portrait(Id));
	}
	return true;
}

SLOP_TEST(FSlopUiFonts, "Ui.FontsShipWithTheirLicences")
bool FSlopUiFonts::RunTest(const FString&)
{
	for (const TCHAR* F : {TEXT("Cinzel-SemiBold.ttf"), TEXT("Cinzel-Bold.ttf"), TEXT("SourceSans3-Regular.ttf"), TEXT("SourceSans3-SemiBold.ttf")})
	{
		TestTrue(F, FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Fonts") / F));
	}
	for (const TCHAR* L : {TEXT("Cinzel-OFL.txt"), TEXT("SourceSans3-OFL.txt")})
	{
		TestTrue(L, FPaths::FileExists(FPaths::ProjectDir() / TEXT("Docs/Licenses") / L));
	}
	return true;
}

// --- What the bars read ------------------------------------------------------------------------------

SLOP_TEST(FSlopUiPlainMeter, "Ui.Hud.AFighterShowsHealthAndRun")
bool FSlopUiPlainMeter::RunTest(const FString&)
{
	FSimHarness H(Fighter(TEXT("ashka")), Fighter(TEXT("gorm")), false);
	FFighter& F = H.F(0);
	F.Health = F.MaxHealth() / 5;
	F.RunMeter = FFighter::RunMax * 0.5;
	const FHudReadout R = HudReadout(F);
	TestEqual("the run meter", (int32)R.Meter, (int32)EHudMeter::Run);
	TestTrue("health is the simulation's", FMath::IsNearlyEqual(R.Health, 0.2f, 0.001f));
	TestTrue("in danger", R.bLow);
	TestTrue("run is the simulation's", FMath::IsNearlyEqual(R.Run, 0.5f, 0.001f));
	F.Health = F.MaxHealth();
	TestFalse("full health is not", HudReadout(F).bLow);
	F.Health = 0;
	TestEqual("no health is an empty bar, not a negative one", HudReadout(F).Health, 0.f);
	return true;
}

SLOP_TEST(FSlopUiQhiMeter, "Ui.Hud.TheGrandmastersBodyShowsQhiNotRun")
bool FSlopUiQhiMeter::RunTest(const FString&)
{
	FGmMatch H(TEXT("ashka"));
	const FHudReadout R = HudReadout(H.Gm());
	TestEqual("Qhi alone in his own form", (int32)R.Meter, (int32)EHudMeter::Qhi);
	TestFalse("not borrowed", R.bBorrowed);
	TestTrue("the mark stands at the half a shapeshift costs", FMath::IsNearlyEqual(R.Mark, (float)FQhiRules::ShapeshiftShare, 0.001f));
	TestEqual("exactly half", (double)R.Mark, 0.5);
	TestTrue("his meter is his own", FMath::IsNearlyEqual(R.Qhi, (float)(H.Gm().Qhi / H.Gm().QhiRules().Max), 0.001f));
	return true;
}

SLOP_TEST(FSlopUiBorrowed, "Ui.Hud.ABorrowedShapeShowsItsRunAndTheQhiHoldingIt")
bool FSlopUiBorrowed::RunTest(const FString&)
{
	FGmMatch H(TEXT("ashka"));
	if (!TestTrue("borrowed", H.Borrow()))
	{
		return false;
	}
	const FHudReadout R = HudReadout(H.Gm());
	TestEqual("both meters", (int32)R.Meter, (int32)EHudMeter::RunAndQhi);
	TestTrue("flagged borrowed", R.bBorrowed);
	TestTrue("the shape's run", R.Run > 0.f);
	TestTrue("and the Qhi left after its half", R.Qhi > 0.f && R.Qhi <= 0.51f);
	return true;
}

SLOP_TEST(FSlopUiSpent, "Ui.Hud.SpentQhiMarksWhereItRecentres")
bool FSlopUiSpent::RunTest(const FString&)
{
	FGmMatch H(TEXT("ashka"));
	H.Gm().Form = EForm::Spent;
	H.Gm().Qhi = 0.0;
	const FHudReadout R = HudReadout(H.Gm());
	TestTrue("spent", R.bSpent);
	TestEqual("still Qhi", (int32)R.Meter, (int32)EHudMeter::Qhi);
	TestEqual("an empty bar", R.Qhi, 0.f);
	TestTrue("the mark is the recentring point", FMath::IsNearlyEqual(R.Mark, (float)(H.Gm().QhiRules().RecoverAt / H.Gm().QhiRules().Max), 0.001f));
	return true;
}

SLOP_TEST(FSlopUiVessel, "Ui.Hud.TheGuardianShowsOnlyHisVessel")
bool FSlopUiVessel::RunTest(const FString&)
{
	FSimHarness H(Fighter(TEXT("ashka")), Fighter(TEXT("hollis")), false);
	FFighter& F = H.F(1);
	F.bGuardian = true;
	TestEqual("nothing while it lies empty", (int32)HudReadout(F).Meter, (int32)EHudMeter::None);
	F.bCharging = true;
	F.Charge = 0.4;
	FHudReadout R = HudReadout(F);
	TestEqual("its gauge while it fills", (int32)R.Meter, (int32)EHudMeter::Vessel);
	TestTrue("as far as it has filled", FMath::IsNearlyEqual(R.Vessel, 0.4f, 0.001f));
	F.bCharging = false;
	F.VentT = 30;
	TestEqual("broken", (int32)HudReadout(F).Meter, (int32)EHudMeter::VesselBroken);
	return true;
}

// --- The staging -------------------------------------------------------------------------------------

// Two identical fights but for how long the first intro is held; the same presses from "fight".
static FString FightOutcome(int32 Hold, bool bEndEarly)
{
	FFightSim Sim;
	Sim.bTraining = false;
	Sim.bIntro = true;
	Sim.Setup(Fighter(TEXT("ashka")), Fighter(TEXT("bram")));
	Sim.IntroHold = Hold;
	FPadState Pads[2];
	int32 Held = 0;
	while (Sim.Phase != EPhase::Fight)
	{
		if (bEndEarly && ++Held == 40)
		{
			Sim.EndIntro();
		}
		Sim.Step(Pads);
	}
	const int32 Start = Sim.TickCount;
	for (int32 T = 0; T < 600; ++T)
	{
		FPadState P[2];
		const int32 Press = T % 40 == 5 ? BtnHP : (T % 40 == 25 ? BtnLK : 0);
		P[0] = {T % 80 < 30 ? PadRight : 0, Press, Press};
		P[1] = {0, T % 60 < 10 ? BtnBL : 0, 0};
		Sim.Step(P);
	}
	return FString::Printf(TEXT("%d %d %.4f %.4f %d %d"), Sim.Fighters[0].Health, Sim.Fighters[1].Health, Sim.Fighters[0].Pos.X, Sim.Fighters[1].Pos.X, Sim.Timer, Sim.TickCount - Start);
}

SLOP_TEST(FSlopHeldIntro, "Rounds.HeldIntroChangesNothingInTheFight")
bool FSlopHeldIntro::RunTest(const FString&)
{
	const FString Plain = FightOutcome(0, false);
	TestEqual("held for the whole staging", FightOutcome(300, false), Plain);
	TestEqual("skipped partway through", FightOutcome(3600, true), Plain);
	return true;
}

SLOP_TEST(FSlopHoldEnds, "Rounds.HeldIntroEndsAtOnceWhenAsked")
bool FSlopHoldEnds::RunTest(const FString&)
{
	FFightSim Sim;
	Sim.bTraining = false;
	Sim.bIntro = true;
	Sim.Setup(Fighter(TEXT("ashka")), Fighter(TEXT("gorm")));
	Sim.IntroHold = 3600;
	FPadState Pads[2];
	for (int32 T = 0; T < FFightSim::IntroTicks + 60; ++T)
	{
		Sim.Step(Pads);
	}
	TestTrue("the hold holds", Sim.Phase == EPhase::Intro);
	Sim.EndIntro();
	Sim.Step(Pads);
	TestTrue("and lets go on the next tick", Sim.Phase == EPhase::Ready);
	return true;
}

// --- Names -------------------------------------------------------------------------------------------

// Whether words a player can read use a retired name. Identifiers are not words: a word boundary keeps
// out SlopUI and MortalSlopbat, and Slopperfall is another game. Docs/Architecture.md lists what keeps
// the old names on purpose.
static bool UsesRetiredName(const FString& Words)
{
	static const FRegexPattern Retired(TEXT("\\bmortal\\s+slopbat\\b|\\bslopbat\\b|sloptalit|\\bslop\\b"), ERegexPatternFlags::CaseInsensitive);
	FRegexMatcher M(Retired, Words);
	return M.FindNext();
}

static void ScanJson(const TSharedPtr<FJsonValue>& V, const FString& Where, TArray<FString>& Found, int32& Seen)
{
	if (!V.IsValid())
	{
		return;
	}
	if (V->Type == EJson::Object)
	{
		for (const auto& KV : V->AsObject()->Values)
		{
			ScanJson(KV.Value, Where + TEXT("/") + KV.Key, Found, Seen);
		}
	}
	else if (V->Type == EJson::Array)
	{
		for (const TSharedPtr<FJsonValue>& E : V->AsArray())
		{
			ScanJson(E, Where, Found, Seen);
		}
	}
	else if (V->Type == EJson::String)
	{
		++Seen;
		// The Fragment's fighter id, which data files cross-reference.
		if (V->AsString() != TEXT("slopbat") && UsesRetiredName(V->AsString()))
		{
			Found.Add(Where + TEXT(": ") + V->AsString());
		}
	}
}

SLOP_TEST(FSlopRetiredNames, "Content.NoRetiredNamesReachThePlayer")
bool FSlopRetiredNames::RunTest(const FString&)
{
	for (const TCHAR* Bad : {TEXT("SLOPTALITY"), TEXT("Stage Sloptality"), TEXT("the Slopbat"), TEXT("Mortal Slopbat"), TEXT("a lake of slop")})
	{
		TestTrue(FString::Printf(TEXT("the scan would catch \"%s\""), Bad), UsesRetiredName(Bad));
	}
	for (const TCHAR* Fine : {TEXT("Slopperfall"), TEXT("SlopUI"), TEXT("~/.local/share/MortalSlopbat/UE5"), TEXT("slopbat_worm"), TEXT("HEARTED")})
	{
		TestFalse(FString::Printf(TEXT("the scan leaves \"%s\""), Fine), UsesRetiredName(Fine));
	}
	TArray<FString> Found;
	int32 Seen = 0;
	// The game's data: every line of the story, every trial, every fighter's and move's text.
	const FString DataDir = FPaths::ProjectContentDir() / TEXT("Data");
	TArray<FString> Bodies;
	IFileManager::Get().FindFiles(Bodies, *(DataDir / TEXT("fighters/*.json")), true, false);
	TestTrue("every fighter's file", Bodies.Num() >= 9);
	TArray<FString> Data = {DataDir / TEXT("story.json"), DataDir / TEXT("trials.json")};
	for (const FString& Body : Bodies)
	{
		Data.Add(DataDir / TEXT("fighters") / Body);
	}
	for (const FString& F : Data)
	{
		TSharedPtr<FJsonObject> J = FContent::LoadJson(F);
		TestTrue(FPaths::GetCleanFilename(F), J.IsValid());
		ScanJson(MakeShared<FJsonValueObject>(J), FPaths::GetCleanFilename(F), Found, Seen);
	}
	// Every string in the game's code, which is where the menus, calls and stage words live.
	TArray<FString> Code;
	IFileManager::Get().FindFilesRecursive(Code, *(FPaths::GameSourceDir() / TEXT("MortalSlopbat")), TEXT("*.cpp"), true, false);
	const FRegexPattern Literal(TEXT("\"((?:[^\"\\\\]|\\\\.)*)\""));
	for (const FString& F : Code)
	{
		if (F.Contains(TEXT("/Tests/")))
		{
			continue;
		}
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *F);
		for (int32 L = 0; L < Lines.Num(); ++L)
		{
			if (!Lines[L].Contains(TEXT("slop")) || Lines[L].TrimStart().StartsWith(TEXT("//")) || Lines[L].TrimStart().StartsWith(TEXT("#include")))
			{
				continue;
			}
			FRegexMatcher M(Literal, Lines[L]);
			while (M.FindNext())
			{
				++Seen;
				const FString S = M.GetCaptureGroup(1);
				// The internal keys Docs/Architecture.md names, and engine paths, are not read by anyone.
				const bool bInternal = S == TEXT("slop") || S == TEXT("sloptality") || S == TEXT("slopbat") || S.StartsWith(TEXT("/Game/Slop")) || S.Contains(TEXT("`Slop."));
				if (!bInternal && UsesRetiredName(S))
				{
					Found.Add(FString::Printf(TEXT("%s:%d: %s"), *FPaths::GetCleanFilename(F), L + 1, *S));
				}
			}
		}
	}
	// What ships beside the game.
	for (const TCHAR* F : {TEXT("Docs/Player-README.txt"), TEXT("LICENSE")})
	{
		FString Text;
		TestTrue(F, FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / F)));
		++Seen;
		if (UsesRetiredName(Text))
		{
			Found.Add(F);
		}
	}
	TestTrue(FString::Printf(TEXT("the scan read the game's text (%d strings)"), Seen), Seen > 1000);
	for (const FString& F : Found)
	{
		AddError(TEXT("A retired name: ") + F);
	}
	return true;
}

// --- Speakers and screens ----------------------------------------------------------------------------

SLOP_TEST(FSlopSpeakers, "Ui.Audio.TheMixFollowsTheSpeakers")
bool FSlopSpeakers::RunTest(const FString&)
{
	// TV speakers, headphones and stereo speakers, a 2.1 soundbar: the plain mix down the middle.
	for (int32 Channels : {1, 2, 3})
	{
		TestFalse(FString::Printf(TEXT("automatic on %d channels"), Channels), USlopAudio::WantsSurround(TEXT("auto"), Channels));
	}
	// Anything with rear channels: the placed mix.
	for (int32 Channels : {4, 6, 8})
	{
		TestTrue(FString::Printf(TEXT("automatic on %d channels"), Channels), USlopAudio::WantsSurround(TEXT("auto"), Channels));
	}
	TestFalse("stereo insists", USlopAudio::WantsSurround(TEXT("stereo"), 8));
	TestTrue("surround insists", USlopAudio::WantsSurround(TEXT("surround"), 2));
	TestEqual("the middle of the screen is the centre speaker", USlopAudio::FrontAzimuth(0.f), 0.f);
	TestEqual("the left edge is the front left", USlopAudio::FrontAzimuth(-1.f), -30.f);
	TestEqual("the right edge is the front right", USlopAudio::FrontAzimuth(1.f), 30.f);
	TestEqual("past the edge stays there", USlopAudio::FrontAzimuth(3.f), 30.f);
	return true;
}

SLOP_TEST(FSlopButtonsIn, "Ui.MoveNotationShowsTheButtonsInUse")
bool FSlopButtonsIn::RunTest(const FString&)
{
	USlopGameInstance* GI = NewObject<USlopGameInstance>();
	GI->Router = MakeShared<Slop::FSlopInputRouter>(GI->Settings);
	const FString Notation = TEXT("↓ → + HP, RN + LK");
	GI->Settings.Glyphs = TEXT("playstation");
	TestEqual("PlayStation", SlopUI::ButtonsIn(GI, Notation), FString(TEXT("↓ → + △, L1 + ✕")));
	GI->Settings.Glyphs = TEXT("xbox");
	TestEqual("Xbox", SlopUI::ButtonsIn(GI, Notation), FString(TEXT("↓ → + Y, LB + A")));
	GI->Settings.Glyphs = TEXT("pc");
	TestEqual("PC, the left half", SlopUI::ButtonsIn(GI, Notation), FString(TEXT("↓ → + U, K + L")));
	TestEqual("PC, the right half", SlopUI::ButtonsIn(GI, TEXT("HP"), 1), SlopUI::KeyName(GI->Settings.Keys[1][(int32)Slop::EGameAction::HP], Slop::EGlyphs::Keyboard).ToString());
	TestEqual("only whole codes change", SlopUI::ButtonsIn(GI, TEXT("Air SHP HP2")), FString(TEXT("Air SHP HP2")));
	return true;
}

SLOP_TEST(FSlopSinkReport, "Ui.Audio.TheDefaultOutputIsReadFromTheSoundServer")
bool FSlopSinkReport::RunTest(const FString&)
{
	// A real report's shape: the default output's own channels count, not the other device's.
	const FString Report = TEXT("Server Name: PulseAudio (on PipeWire 1.0.5)\n")
		TEXT("Default Sink: alsa_output.tv.hdmi-stereo\nDefault Source: none\n")
		TEXT("1143\talsa_output.amp.hdmi-surround\tPipeWire\ts32le 6ch 48000Hz\tIDLE\n")
		TEXT("1144\talsa_output.tv.hdmi-stereo\tPipeWire\ts32le 2ch 48000Hz\tRUNNING\n");
	TestEqual("the default is stereo", USlopAudio::DefaultSinkChannels(Report), 2);
	TestEqual("the default is 5.1", USlopAudio::DefaultSinkChannels(Report.Replace(TEXT("Default Sink: alsa_output.tv.hdmi-stereo"), TEXT("Default Sink: alsa_output.amp.hdmi-surround"))), 6);
	TestEqual("no sound server: stereo", USlopAudio::DefaultSinkChannels(FString()), 2);
	TestEqual("a default it cannot find: stereo", USlopAudio::DefaultSinkChannels(TEXT("Default Sink: gone\n1\tother\tPipeWire\ts32le 8ch 48000Hz\tIDLE\n")), 2);
	return true;
}

SLOP_TEST(FSlopMargin, "Ui.ScreenMarginKeepsTheDesignInside")
bool FSlopMargin::RunTest(const FString&)
{
	const FVector2D Shapes[] = {{3840, 2160}, {1920, 1080}, {3440, 1440}, {1440, 1080}};
	SlopUI::SetScreenMargin(0.05f);
	for (const FVector2D& S : Shapes)
	{
		const SlopUI::FStageFrame F = SlopUI::StageFrame(S);
		const FString Name = FString::Printf(TEXT("%.0fx%.0f"), S.X, S.Y);
		TestTrue(Name + TEXT(": the design still fits"), F.Size.X >= 1919.9 && F.Size.Y >= 1079.9);
		TestTrue(Name + TEXT(": 5% clear at the left"), FMath::IsNearlyEqual(F.Origin.X, S.X * 0.05, 0.01));
		TestTrue(Name + TEXT(": and as much at the right"), FMath::IsNearlyEqual(F.Origin.X + F.Size.X * F.Scale, S.X * 0.95, 0.5));
		TestTrue(Name + TEXT(": and at the foot"), FMath::IsNearlyEqual(F.Origin.Y + F.Size.Y * F.Scale, S.Y * 0.95, 0.5));
	}
	SlopUI::SetScreenMargin(1.f);
	TestTrue("no more than the setting's limit", FMath::IsNearlyEqual(SlopUI::StageFrame(FVector2D(1000, 1000)).Origin.X, 50.0, 0.01));
	SlopUI::SetScreenMargin(0.f);
	TestTrue("none by default", SlopUI::StageFrame(FVector2D(3840, 2160)).Origin.IsNearlyZero());
	return true;
}

// --- Settings ----------------------------------------------------------------------------------------

SLOP_TEST(FSlopTvSettings, "Save.SpeakerAndScreenSettingsAreSafeInOlderFiles")
bool FSlopTvSettings::RunTest(const FString&)
{
	FSlopSettings S;
	TestEqual("automatic by default", S.Speakers, FString(TEXT("auto")));
	TestTrue("the main display by default", S.Screen.IsEmpty());
	S.Speakers = TEXT("surround");
	S.Screen = TEXT("58R6+");
	S.ScreenMargin = 0.03;
	FSlopSettings R;
	R.FromJson(S.ToJson());
	TestEqual("speakers kept", R.Speakers, FString(TEXT("surround")));
	TestEqual("screen kept", R.Screen, FString(TEXT("58R6+")));
	TestEqual("margin kept", R.ScreenMargin, 0.03);
	FSlopSettings Old;
	Old.FromJson(FContent::ParseJson(TEXT(R"({"version": 2, "speakers": "dolby", "screen_margin": 0.5})")));
	TestEqual("nonsense speakers", Old.Speakers, FString(TEXT("auto")));
	TestEqual("a margin too large", Old.ScreenMargin, 0.05);
	return true;
}

SLOP_TEST(FSlopIntroSetting, "Save.PreFightIntroSettingIsSafeInOlderFiles")
bool FSlopIntroSetting::RunTest(const FString&)
{
	FSlopSettings S;
	TestEqual("full by default", S.IntroLength, FString(TEXT("full")));
	S.IntroLength = TEXT("short");
	FSlopSettings R;
	R.FromJson(S.ToJson());
	TestEqual("kept", R.IntroLength, FString(TEXT("short")));
	FSlopSettings Old;
	Old.FromJson(FContent::ParseJson(TEXT(R"({"version": 2, "frame_cap": 60})")));
	TestEqual("a file from before the setting", Old.IntroLength, FString(TEXT("full")));
	Old.FromJson(FContent::ParseJson(TEXT(R"({"version": 2, "intro": "sideways"})")));
	TestEqual("nonsense", Old.IntroLength, FString(TEXT("full")));
	return true;
}
}

#undef SLOP_TEST

#endif
