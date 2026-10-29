#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SlopSimHarness.h"
#include "AI/SlopAI.h"
#include "Core/SlopFlow.h"
#include "Core/SlopSave.h"
#include "Serialization/JsonSerializer.h"

// The two fighters added after the six: Wren Sallow, the seventh of the starting roster, and the
// Grandmaster, who is unlocked. His rules are checked here as the owner set them: a Qhi
// meter in place of the run meter, a shapeshift into the opponent facing him for exactly half the
// meter, a copy stronger than its original, a run meter only while he wears one, and a spent,
// drunken body when the Qhi is gone.
namespace SlopRosterTestSuite
{
using namespace Slop;
using namespace Slop::Test;

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

const FFighterDef* Fighter(const TCHAR* Id) { return GetContent().Fighter(FName(Id)); }

// Damage the right-hand fighter has taken.
int32 Taken(FSimHarness& H) { return H.F(1).MaxHealth() - H.F(1).Health; }

// A match against `Opponent` with real rules (no infinite meters), the Grandmaster on the left.
struct FGmMatch : FSimHarness
{
	explicit FGmMatch(const TCHAR* Opponent, const TCHAR* Self = TEXT("grandmaster")) : FSimHarness(Fighter(Self), Fighter(Opponent), false)
	{
		Sim.RoundLength = 0;
		Place(3.0);
	}
	FFighter& Gm() { return F(0); }
	// D,B,F + Block: Borrowed Face (and, in a borrowed shape, Own Face).
	void Shapeshift() { Motion(0, {D, D | L, L, R}, BL); }
	const FSimEvent* FormEvent(FName Kind, int32 Since = 0) const
	{
		for (int32 I = Since; I < Log.Num(); ++I)
		{
			if (Log[I].Type == EEvent::Form && Log[I].Kind == Kind)
			{
				return &Log[I];
			}
		}
		return nullptr;
	}
	bool Borrow()
	{
		Shapeshift();
		return Until([this]() { return Gm().Form == EForm::Borrowed; }, 60);
	}
	// The opponent's high punch from where it lands.
	int32 JabDamageOnGm()
	{
		Place(0.7);
		const int32 Was = Gm().Health;
		Step(0, 0, 0, HP);
		Idle(20);
		return Was - Gm().Health;
	}
};

SLOP_TEST(FSlopRosterSevenAndSecret, "Fighters.RosterOfSevenAndTheSecretGrandmaster")
bool FSlopRosterSevenAndSecret::RunTest(const FString&)
{
	const TArray<FName>& R = FContent::Roster();
	TestEqual("seven starting fighters", R.Num(), 7);
	for (const TCHAR* Id : {TEXT("ashka"), TEXT("bram"), TEXT("gorm"), TEXT("hollis"), TEXT("mirela"), TEXT("nix"), TEXT("wren")})
	{
		TestTrue(FString::Printf(TEXT("%s starts selectable"), Id), R.Contains(FName(Id)) && Fighter(Id) != nullptr);
	}
	TestFalse("the Grandmaster is not a starting fighter", R.Contains(FName(TEXT("grandmaster"))));
	TestTrue("the Grandmaster is the secret fighter", FContent::Secret().Contains(FName(TEXT("grandmaster"))) && Fighter(TEXT("grandmaster")));
	TestTrue("the Slopbat and the stranger are still there", Fighter(TEXT("slopbat")) && Fighter(TEXT("daxion")) && Fighter(TEXT("slopbat"))->bBoss);
	TestEqual("no data errors", GetContent().Errors.Num(), 0);
	return true;
}

SLOP_TEST(FSlopGmQhiNotRun, "Fighters.Grandmaster.OwnBodyHasQhiAndNoRunMeter")
bool FSlopGmQhiNotRun::RunTest(const FString&)
{
	FGmMatch H(TEXT("ashka"));
	FFighter& G = H.Gm();
	TestTrue("his own body uses Qhi", G.UsesQhi() && G.Form == EForm::Own);
	TestFalse("and has no run meter", G.HasRunMeter());
	TestEqual("Qhi starts full", G.Qhi, 100.0);
	// Run + forward is his Qhi glide: it moves him and drains Qhi, never a run meter.
	H.Hold[0] = RN;
	H.Idle(40, R);
	H.Hold[0] = 0;
	TestTrue("the glide drains Qhi", G.Qhi < 95.0);
	TestEqual("the unused run meter is untouched", G.RunMeter, FFighter::RunMax);
	TestFalse("Ashka has no Qhi", H.F(1).UsesQhi());
	return true;
}

SLOP_TEST(FSlopGmCopiesEveryone, "Fighters.Grandmaster.BorrowsEveryStandardOpponentForHalfTheQhi")
bool FSlopGmCopiesEveryone::RunTest(const FString&)
{
	for (const FName& Id : FContent::Roster())
	{
		FGmMatch H(*Id.ToString());
		FFighter& G = H.Gm();
		const FFighterDef* Target = Fighter(*Id.ToString());
		const bool bTook = H.Borrow();
		const FString Who = Id.ToString();
		if (!TestTrue(Who + TEXT(": the shapeshift takes"), bTook))
		{
			continue;
		}
		const FSimEvent* E = H.FormEvent(TEXT("borrowed"));
		TestTrue(Who + TEXT(": the event names the opponent"), E && E->Move == Id);
		TestTrue(Who + TEXT(": the shape is the current opponent's"), G.Def->CopyOf == Target && G.BaseDef->Id == FName(TEXT("grandmaster")));
		// Paid on the frame it took: exactly half of the meter's capacity, before any upkeep.
		TestEqual(Who + TEXT(": half the Qhi spent"), G.Qhi, 100.0 - G.QhiRules().ShapeshiftCost());
		TestTrue(Who + TEXT(": a borrowed shape has a run meter, full"), G.HasRunMeter() && G.RunMeter == FFighter::RunMax);
		// The copy fights with the original's whole framework.
		bool bAllMoves = true;
		for (const FCommand& C : Target->Commands)
		{
			bAllMoves &= G.Def->Move(C.Move) != nullptr;
		}
		TestTrue(Who + TEXT(": every one of the opponent's commands"), bAllMoves);
		TestTrue(Who + TEXT(": the body and height are the opponent's"), G.Def->Body == Target->Body && G.Def->Height == Target->Height);
		// And stronger than it, by Qhi.
		TestTrue(Who + TEXT(": hits harder"), G.Def->Power > Target->Power && G.Def->Arcana > Target->Arcana);
		TestTrue(Who + TEXT(": takes less"), G.Def->PhysDef < Target->PhysDef && G.Def->MagicDef < Target->MagicDef);
		TestTrue(Who + TEXT(": moves faster"), G.Def->WalkF > Target->WalkF && G.Def->RunSpeed > Target->RunSpeed);
		TestFalse(Who + TEXT(": no finisher of the opponent's"), G.Def->bHasFinisher);
		TestEqual(Who + TEXT(": health stays his own"), G.MaxHealth(), Fighter(TEXT("grandmaster"))->Health);
	}
	return true;
}

SLOP_TEST(FSlopGmCopyStronger, "Fighters.Grandmaster.CopyOutfightsTheOriginal")
bool FSlopGmCopyStronger::RunTest(const FString&)
{
	// The same high punch, from the original and from the copy, into the same fighter.
	for (const FName& Id : FContent::Roster())
	{
		const FFighterDef* Target = Fighter(*Id.ToString());
		FSimHarness Orig(Target, Fighter(TEXT("bram")), false);
		Orig.Sim.RoundLength = 0;
		Orig.Place(0.7);
		Orig.Step(0, HP);
		Orig.Idle(20);
		const int32 ByOriginal = Taken(Orig);

		FGmMatch H(TEXT("bram"));
		H.Gm().SetForm(EForm::Borrowed, H.Sim.ShapeOf(*Target, *H.Gm().BaseDef), TEXT("borrowed"));
		H.Gm().Qhi = 50.0;
		H.Place(0.7);
		H.Step(0, HP);
		H.Idle(20);
		const int32 ByCopy = Taken(H);
		TestTrue(FString::Printf(TEXT("%s: the copy's high punch does more (%d against %d)"), *Id.ToString(), ByCopy, ByOriginal), ByCopy > ByOriginal && ByOriginal > 0);
	}
	return true;
}

SLOP_TEST(FSlopGmNotEnoughQhi, "Fighters.Grandmaster.ShapeshiftNeedsHalfTheMeter")
bool FSlopGmNotEnoughQhi::RunTest(const FString&)
{
	FGmMatch H(TEXT("nix"));
	// Just short of half, and nothing recovering in the moment it takes.
	H.Gm().Qhi = 49.9;
	H.Gm().QhiIdle = 0;
	H.Shapeshift();
	H.Idle(50);
	TestEqual("no shapeshift below half", H.Gm().Form, EForm::Own);
	const FSimEvent* E = H.FormEvent(TEXT("refused"));
	TestTrue("refused aloud, for want of Qhi", E && E->How == FName(TEXT("qhi")));
	TestTrue("nothing paid", H.Gm().Qhi >= 49.9);
	return true;
}

SLOP_TEST(FSlopGmInterrupted, "Fighters.Grandmaster.BrokenShapeshiftCostsLittle")
bool FSlopGmInterrupted::RunTest(const FString&)
{
	FGmMatch H(TEXT("bram"));
	H.Place(0.75);
	H.Shapeshift();
	// Bram jabs him in the long startup.
	H.Step(0, 0, 0, HP);
	H.Idle(60);
	TestEqual("still his own shape", H.Gm().Form, EForm::Own);
	TestNotNull("the shapeshift was broken", H.FormEvent(TEXT("failed")));
	TestNull("and never took", H.FormEvent(TEXT("borrowed")));
	const double Cost = 100.0 - H.Gm().Qhi;
	TestTrue(FString::Printf(TEXT("broken, it costs its small price, not half (%.1f)"), Cost), Cost >= H.Gm().QhiRules().FailCost - 0.01 && Cost < 20.0);
	return true;
}

SLOP_TEST(FSlopGmRevertNoRefund, "Fighters.Grandmaster.RevertingRefundsNothing")
bool FSlopGmRevertNoRefund::RunTest(const FString&)
{
	FGmMatch H(TEXT("mirela"));
	TestTrue("borrowed", H.Borrow());
	H.Idle(40);
	const double Before = H.Gm().Qhi;
	TestTrue("the shape costs upkeep", Before < 50.0);
	const int32 At = H.Log.Num();
	H.Shapeshift();
	H.Until([&H]() { return H.Gm().Form == EForm::Own; }, 60);
	TestNotNull("Own Face", H.FormEvent(TEXT("own"), At));
	TestEqual("back to his own definition", H.Gm().Def, H.Gm().BaseDef);
	TestFalse("and no run meter", H.Gm().HasRunMeter());
	TestTrue(FString::Printf(TEXT("no Qhi comes back with his face (%.1f, was %.1f)"), H.Gm().Qhi, Before), H.Gm().Qhi <= Before);
	// Borrowing again pays the full half again.
	H.Gm().Qhi = 100.0;
	H.Idle(30);
	TestTrue("borrowed again", H.Borrow());
	TestEqual("for the same half", H.Gm().Qhi, 50.0);
	return true;
}

SLOP_TEST(FSlopGmShapeBreaks, "Fighters.Grandmaster.EmptyMeterBreaksTheShapeIntoTheSpentBody")
bool FSlopGmShapeBreaks::RunTest(const FString&)
{
	FGmMatch H(TEXT("gorm"));
	TestTrue("borrowed", H.Borrow());
	H.Gm().Qhi = 0.5;
	H.Idle(30);
	TestNotNull("the shape breaks", H.FormEvent(TEXT("broken")));
	TestEqual("into the spent body", H.Gm().Form, EForm::Spent);
	TestEqual("drunken boxing", H.Gm().Def->Id, FName(TEXT("grandmaster_spent")));
	// The spent body recovers slowly: it stays spent for many seconds.
	int32 Ticks = 0;
	while (H.Gm().Form == EForm::Spent && Ticks < 2000)
	{
		H.Idle(1);
		++Ticks;
	}
	TestTrue(FString::Printf(TEXT("spent for long (%.1f s)"), Ticks / 60.0), Ticks > 8 * 60);
	TestEqual("then recentred in his own body", H.Gm().Form, EForm::Own);
	TestNotNull("aloud", H.FormEvent(TEXT("recentred")));
	TestTrue("with only the Qhi it took to recentre", H.Gm().Qhi < H.Gm().QhiRules().ShapeshiftCost());
	return true;
}

SLOP_TEST(FSlopGmSpentWeak, "Fighters.Grandmaster.SpentBodyIsWeakDrunkenBoxing")
bool FSlopGmSpentWeak::RunTest(const FString&)
{
	const FFighterDef* Own = Fighter(TEXT("grandmaster"));
	const FFighterDef* Spent = Fighter(TEXT("grandmaster_spent"));
	TestTrue("the spent form is his own body", Spent && Spent->Body == Own->Id && Own->SpentForm == Spent);
	TestTrue("weaker defence against blows", Spent->PhysDef > Own->PhysDef);
	TestTrue("and the Qhi's protection is gone", Spent->MagicDef > Own->MagicDef);
	TestTrue("weaker blows", Spent->Power < Own->Power);
	TestTrue("slower feet", Spent->WalkF < Own->WalkF && Spent->JumpVy < Own->JumpVy);
	TestEqual("no meter of any kind", Spent->Resource, EResource::None);
	TestTrue("drunken boxing, not grappling", Spent->Style.Contains(TEXT("Drunken")) && !Spent->Style.Contains(TEXT("Jiu")));
	for (const FCommand& C : Spent->Commands)
	{
		const FMoveDef* M = Spent->Move(C.Move);
		TestTrue(C.Text + TEXT(": no Qhi move when spent"), M && M->QhiCost <= 0.0 && !M->bShapeshift && M->Magic <= 0.0);
	}
	FGmMatch H(TEXT("ashka"));
	H.Gm().Qhi = 0.0;
	H.Idle(5);
	TestEqual("an empty meter spends him", H.Gm().Form, EForm::Spent);
	TestNotNull("aloud", H.FormEvent(TEXT("spent")));
	H.Idle(40);
	H.Place(3.0);
	const int32 At = H.Log.Num();
	H.Motion(0, {D, D | R, R}, HP);
	H.Idle(20);
	TestFalse("no Still Palm when spent", H.Started(0, TEXT("still_palm"), At));
	TestEqual("no projectile", H.Sim.Projectiles.Num(), 0);
	H.Hold[0] = RN;
	H.Idle(20, R);
	H.Hold[0] = 0;
	TestNotEqual("no glide when spent", H.Gm().State, EState::Run);
	H.Idle(30);
	H.Gm().Qhi = 0.0;
	const int32 SpentJab = H.JabDamageOnGm();
	FGmMatch Fresh(TEXT("ashka"));
	const int32 OwnJab = Fresh.JabDamageOnGm();
	TestTrue(FString::Printf(TEXT("a jab hurts the spent body more (%d against %d)"), SpentJab, OwnJab), SpentJab > OwnJab);
	return true;
}

SLOP_TEST(FSlopGmDefences, "Fighters.Grandmaster.WorstPhysicalBestMagicalDefence")
bool FSlopGmDefences::RunTest(const FString&)
{
	const FFighterDef* Gm = Fighter(TEXT("grandmaster"));
	for (const FName& Id : FContent::Roster())
	{
		const FFighterDef* D = Fighter(*Id.ToString());
		TestTrue(Id.ToString() + TEXT(": the Grandmaster's physical defence is the worst"), Gm->PhysDef > D->PhysDef);
		TestTrue(Id.ToString() + TEXT(": his magical defence the best"), Gm->MagicDef < D->MagicDef);
	}
	// Measured: Ashka's jab (physical), Ember Shot (magical) and Wick Knee (mixed), into him and
	// into Bram (defences as authored).
	auto Taken = [](const TCHAR* Victim, TFunction<void(FSimHarness&)> Do)
	{
		FSimHarness H(Fighter(TEXT("ashka")), Fighter(Victim), false);
		H.Sim.RoundLength = 0;
		Do(H);
		return H.F(1).MaxHealth() - H.F(1).Health;
	};
	auto Jab = [](FSimHarness& H) { H.Place(0.7); H.Step(0, HP); H.Idle(20); };
	auto Ember = [](FSimHarness& H) { H.Place(2.0); H.Motion(0, {D, D | R, R}, HP); H.Idle(60); };
	auto Knee = [](FSimHarness& H) { H.Place(1.8); H.Motion(0, {L, R}, HK); H.Idle(60); };
	const int32 JabB = Taken(TEXT("bram"), Jab), JabG = Taken(TEXT("grandmaster"), Jab);
	const int32 EmB = Taken(TEXT("bram"), Ember), EmG = Taken(TEXT("grandmaster"), Ember);
	const int32 KnB = Taken(TEXT("bram"), Knee), KnG = Taken(TEXT("grandmaster"), Knee);
	TestTrue(FString::Printf(TEXT("a jab hurts him more (%d against %d)"), JabG, JabB), JabG > JabB && JabB > 0);
	TestTrue(FString::Printf(TEXT("magic hurts him less (%d against %d)"), EmG, EmB), EmG < EmB && EmG > 0);
	TestEqual("magic: half", EmG, FMath::RoundToInt(EmB * Gm->MagicDef));
	// A mixed blow is weighed by its share: never immune, never the full physical penalty.
	const double Share = Fighter(TEXT("ashka"))->Move(TEXT("wick_knee"))->Magic;
	TestEqual(FString::Printf(TEXT("mixed (%.0f%% magical)"), Share * 100), KnG, FMath::RoundToInt(KnB * ((1.0 - Share) * Gm->PhysDef + Share * Gm->MagicDef)));
	return true;
}

SLOP_TEST(FSlopGmRefusals, "Fighters.Grandmaster.BossesAndMirrorsCannotBeBorrowed")
bool FSlopGmRefusals::RunTest(const FString&)
{
	{
		FGmMatch H(TEXT("slopbat"));
		H.Shapeshift();
		H.Idle(50);
		const FSimEvent* E = H.FormEvent(TEXT("refused"));
		TestTrue("the Slopbat's shape: refused, and said why", E && E->How == FName(TEXT("boss")));
		TestEqual("nothing paid", H.Gm().Qhi, 100.0);
		TestEqual("still his own shape", H.Gm().Form, EForm::Own);
	}
	{
		FGmMatch H(TEXT("daxion"));
		H.Shapeshift();
		H.Idle(50);
		TestEqual("the stranger: no shape to borrow", H.Gm().Form, EForm::Own);
	}
	{
		FGmMatch H(TEXT("grandmaster"));
		H.Shapeshift();
		H.Idle(50);
		const FSimEvent* E = H.FormEvent(TEXT("refused"));
		TestTrue("a mirror: refused", E && E->How == FName(TEXT("mirror")));
		// A Grandmaster wearing a borrowed shape is copied from the shape's source.
		H.F(1).SetForm(EForm::Borrowed, H.Sim.ShapeOf(*Fighter(TEXT("ashka")), *Fighter(TEXT("grandmaster"))), TEXT("borrowed"));
		H.F(1).Qhi = 50.0;
		TestTrue("a copy is copied from its source", H.Gm().BorrowTarget() == Fighter(TEXT("ashka")) && H.Gm().BorrowRefusal().IsNone());
	}
	return true;
}

SLOP_TEST(FSlopGmRoundReset, "Fighters.Grandmaster.EveryRoundStartsInHisOwnShape")
bool FSlopGmRoundReset::RunTest(const FString&)
{
	FGmMatch H(TEXT("hollis"));
	TestTrue("borrowed", H.Borrow());
	H.Sim.RestartRound();
	TestEqual("own shape at the round's start", H.Gm().Def, H.Gm().BaseDef);
	TestEqual("full Qhi", H.Gm().Qhi, 100.0);
	H.Until([&H]() { return H.Sim.Phase == EPhase::Fight; }, 300);
	H.Gm().Qhi = 0.0;
	H.Idle(5);
	TestEqual("spent", H.Gm().Form, EForm::Spent);
	H.Sim.RestartRound();
	TestEqual("own shape again", H.Gm().Form, EForm::Own);
	return true;
}

SLOP_TEST(FSlopGmWinsInOwnShape, "Fighters.Grandmaster.ADecidedFightLetsTheShapeGo")
bool FSlopGmWinsInOwnShape::RunTest(const FString&)
{
	// Borrowed, he lands the deciding blow: the shape is let go once the fight is decided (no
	// refund), so his own finisher, not the opponent's, is his to enter; and a throw or a blow in
	// the shapeshift's startup leaves him in his own shape.
	FGmMatch H(TEXT("nix"));
	H.Sim.RoundsToWin = 1;
	TestTrue("borrowed", H.Borrow());
	H.Idle(12);
	H.F(1).Health = 1;
	H.Place(0.7);
	const double Before = H.Gm().Qhi;
	H.Step(0, HP);
	H.Until([&H]() { return H.Sim.Phase != EPhase::Fight && H.Gm().Form == EForm::Own; }, 240);
	TestEqual("own shape once the fight is decided", H.Gm().Form, EForm::Own);
	TestTrue("with no Qhi back", H.Gm().Qhi <= Before);
	TestTrue("his own finisher", H.Gm().Def->bHasFinisher && H.Gm().Def->FinisherCmd.Move == FName(TEXT("still_water")));

	FGmMatch T(TEXT("bram"));
	T.Place(0.7);
	T.Shapeshift();
	T.Step(0, 0, R, LP);
	T.Idle(80);
	TestNull("thrown in the startup: no shape", T.FormEvent(TEXT("borrowed")));
	TestNotNull("the shapeshift is broken", T.FormEvent(TEXT("failed")));
	return true;
}

SLOP_TEST(FSlopGmNoLoops, "Fighters.Grandmaster.NoCancelIntoAChangeOfShapeAndNoFarming")
bool FSlopGmNoLoops::RunTest(const FString&)
{
	// Ashka's Elbow Rush can cancel into specials once it lands: never into Own Face.
	FGmMatch H(TEXT("bram"));
	H.Gm().SetForm(EForm::Borrowed, H.Sim.ShapeOf(*Fighter(TEXT("ashka")), *Fighter(TEXT("grandmaster"))), TEXT("borrowed"));
	H.Gm().Qhi = 50.0;
	H.Place(1.2);
	H.Motion(0, {R, 0, R}, LP);
	H.Until([&H]() { return H.Gm().MoveContact == FName(TEXT("hit")); }, 30);
	H.Shapeshift();
	TestEqual("no reversal out of a cancel", H.Gm().Form, EForm::Borrowed);
	// Caught blows give Qhi back at most once in a while, and never to a borrowed shape.
	FFighter& G = H.Gm();
	G.SetForm(EForm::Own, G.BaseDef, TEXT("own"));
	G.Qhi = 20.0;
	G.QhiGainAt = 0;
	G.DefendGain();
	G.DefendGain();
	G.DefendGain();
	TestEqual("one gain at a time", G.Qhi, 20.0 + G.QhiRules().DefendGain);
	G.SetForm(EForm::Borrowed, H.Sim.ShapeOf(*Fighter(TEXT("ashka")), *G.BaseDef), TEXT("borrowed"));
	G.QhiGainAt = 0;
	const double Q = G.Qhi;
	G.DefendGain();
	TestEqual("a borrowed shape earns no Qhi", G.Qhi, Q);
	return true;
}

SLOP_TEST(FSlopGmWeaveAndWard, "Fighters.Grandmaster.WeaveSlipsHighsAndTheWardBreaksProjectiles")
bool FSlopGmWeaveAndWard::RunTest(const FString&)
{
	{
		FGmMatch H(TEXT("bram"));
		H.Place(0.75);
		H.Motion(0, {L, D | L, D}, LP);
		H.Step(0, 0, 0, HP);
		H.Idle(30);
		const FSimEvent* E = H.Find(EEvent::Parry);
		TestTrue("the weave slips Bram's high punch", E && E->Kind == FName(TEXT("weave")) && H.Gm().Health == H.Gm().MaxHealth());
		H.Place(0.75);
		const int32 Was = H.Gm().Health;
		H.Motion(0, {L, D | L, D}, LP);
		H.Step(D, 0, D, LK);
		H.Idle(30);
		TestTrue("but a low gets through it", H.Gm().Health < Was);
	}
	{
		FGmMatch H(TEXT("ashka"));
		H.Place(2.5);
		H.Motion(0, {D, 0, D}, BL);
		const int32 Was = H.Gm().Health;
		H.Motion(1, {D, D | L, L}, HP);
		H.Idle(60);
		TestEqual("the ward breaks Ember Shot", H.Gm().Health, Was);
		const FSimEvent* E = H.Find(EEvent::Parry);
		TestTrue("as a ward", E && E->bProj && E->Kind == FName(TEXT("ward")));
		TestTrue("and the ward cost Qhi", H.Gm().Qhi < 100.0);
	}
	return true;
}

SLOP_TEST(FSlopWrenFeint, "Fighters.Wren.FeintCancelsIntoAnythingButItself")
bool FSlopWrenFeint::RunTest(const FString&)
{
	FSimHarness H(Fighter(TEXT("wren")), Fighter(TEXT("gorm")), false);
	H.Sim.RoundLength = 0;
	H.Place(0.8);
	H.Motion(0, {D, D | L, L}, LK);
	TestTrue("Wisp Feint", H.Started(0, TEXT("wisp_feint")));
	H.Idle(6);
	H.Step(0, LK);
	TestTrue("cancelled into a kick, no contact needed", H.Started(0, TEXT("lk")));
	H.Idle(40);
	H.Place(0.8);
	const int32 At = H.Log.Num();
	H.Motion(0, {D, D | L, L}, LK);
	H.Idle(6);
	H.Motion(0, {D, D | L, L}, LK);
	int32 Feints = 0;
	for (int32 I = At; I < H.Log.Num(); ++I)
	{
		Feints += H.Log[I].Type == EEvent::Move && H.Log[I].Move == FName(TEXT("wisp_feint")) ? 1 : 0;
	}
	TestEqual("a feint never feints into itself", Feints, 1);
	return true;
}

SLOP_TEST(FSlopWrenArmlock, "Fighters.Wren.BarTheDoorCatchesIntoAnArmlock")
bool FSlopWrenArmlock::RunTest(const FString&)
{
	FSimHarness H(Fighter(TEXT("wren")), Fighter(TEXT("bram")), false);
	H.Sim.RoundLength = 0;
	H.Place(0.75);
	H.Motion(0, {L, D | L, D}, HP);
	H.Step(0, 0, 0, HP);
	H.Idle(6);
	TestTrue("Bram's jab is caught", H.Find(EEvent::Parry) != nullptr);
	TestEqual("and held", H.F(1).State, EState::Thrown);
	// Mashing does not break a lock taken from a caught blow.
	for (int32 I = 0; I < 24; ++I)
	{
		H.Step(0, 0, 0, I % 2 ? LP : HP);
	}
	TestEqual("no break", H.Count(EEvent::ThrowBreak), 0);
	H.Idle(90);
	const FSimEvent* E = nullptr;
	for (const FSimEvent& X : H.Log)
	{
		if (X.Type == EEvent::Hit && X.Move == FName(TEXT("wren_armlock")))
		{
			E = &X;
		}
	}
	TestTrue("the armlock does its damage", E && E->Damage > 0);
	// A low is not caught: it hits her.
	H.Place(0.75);
	const int32 Was = H.F(0).Health;
	H.Motion(0, {L, D | L, D}, HP);
	H.Step(0, 0, D, LK);
	H.Idle(30);
	TestTrue("a low beats it", H.F(0).Health < Was);
	return true;
}

SLOP_TEST(FSlopWrenFlicker, "Fighters.Wren.FlickerStepPassesThroughProjectiles")
bool FSlopWrenFlicker::RunTest(const FString&)
{
	FSimHarness H(Fighter(TEXT("wren")), Fighter(TEXT("ashka")), false);
	H.Sim.RoundLength = 0;
	H.Place(3.4);
	H.Motion(1, {D, D | L, L}, HP);
	// Wait until the Ember Shot is close, then flicker through it.
	H.Until([&H]() { return H.Sim.Projectiles.Num() > 0 && FMath::Abs(H.Sim.Projectiles[0].Pos.X - H.F(0).Pos.X) < 1.3; }, 80);
	const int32 Was = H.F(0).Health;
	H.Motion(0, {R, 0, R}, LK);
	H.Idle(12);
	TestEqual("not hit while flickering", H.F(0).Health, Was);
	TestTrue("and well forward", H.F(0).Pos.X > -1.7 + 1.0);
	return true;
}

SLOP_TEST(FSlopSecretChallenge, "Fighters.Grandmaster.UnlockedByAnArcadeWithoutAContinue")
bool FSlopSecretChallenge::RunTest(const FString&)
{
	FGameFlow F;
	F.StartArcade(TEXT("wren"), 0, F.PlanArcade(TEXT("wren"), 0, 7));
	while (F.ArcadeAdvance(false, 0.0))
	{
	}
	TestEqual("at the Slopbat, the run complete", F.CurrentRung()->Kind, FName(TEXT("boss")));
	TestTrue("earned without a continue", F.SecretEarned());
	F.AddSecretRung();
	TestTrue("the old master is one more rung", F.OnSecretRung() && F.Config.P2 == FName(TEXT("grandmaster")));
	TestEqual("at Master, whatever the tower", F.Config.Difficulty, 2);
	TestEqual("with the CPU's tier beyond it", F.Config.CpuTier, 3);
	TestFalse("and the run ends after him", F.ArcadeAdvance(false, 0.0));

	FGameFlow C;
	C.StartArcade(TEXT("bram"), 1, C.PlanArcade(TEXT("bram"), 1, 3));
	C.ArcadeRetry();
	while (C.ArcadeAdvance(false, 0.0))
	{
	}
	TestFalse("a continue used: not earned", C.SecretEarned());

	FGameFlow G;
	G.StartArcade(TEXT("grandmaster"), 2, G.PlanArcade(TEXT("grandmaster"), 2, 5));
	while (G.ArcadeAdvance(false, 0.0))
	{
	}
	TestFalse("the Grandmaster does not meet himself", G.SecretEarned());

	// The unlock is a saved flag: a fresh profile has it locked, the saved file keeps it.
	FSlopProgress P;
	P.Reset();
	TestFalse("locked at first", P.IsUnlocked(TEXT("fighter:grandmaster")));
	P.Unlock(TEXT("fighter:grandmaster"));
	FString Text;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(P.Data.ToSharedRef(), W);
	FSlopProgress Q;
	Q.Data = FContent::ParseJson(Text);
	TestTrue("the unlock survives a save and load", Q.Data.IsValid() && Q.IsUnlocked(TEXT("fighter:grandmaster")));
	return true;
}

SLOP_TEST(FSlopGmCpuFair, "AI.GrandmasterKeepsTheQhiRules")
bool FSlopGmCpuFair::RunTest(const FString&)
{
	// The CPU plays him by the same rules: it borrows only with the price in hand, pays it, keeps
	// its meter honest, and a Master CPU gives the shape back before it breaks more often than not.
	int32 Borrows = 0, Breaks = 0, Reverts = 0, Matches = 0;
	for (const TCHAR* Opp : {TEXT("ashka"), TEXT("gorm"), TEXT("wren"), TEXT("mirela")})
	{
		for (int32 Seed = 1; Seed <= 2; ++Seed)
		{
			FSimHarness H(Fighter(TEXT("grandmaster")), Fighter(Opp), false);
			FCpuBrain A(H.Sim, 0, 2, Seed * 17);
			FCpuBrain B(H.Sim, 1, 1, Seed * 29);
			++Matches;
			double Prev = H.F(0).Qhi;
			for (int32 T = 0; T < 60 * 90 && H.Sim.Phase != EPhase::MatchEnd; ++T)
			{
				FPadState Pads[2] = {A.Think(), B.Think()};
				const int32 At = H.Log.Num();
				H.Sim.Step(Pads);
				H.Log.Append(H.Sim.Events);
				const FFighter& G = H.F(0);
				if (!TestTrue("Qhi stays within the meter", G.Qhi >= 0.0 && G.Qhi <= G.QhiRules().Max))
				{
					return false;
				}
				for (int32 I = At; I < H.Log.Num(); ++I)
				{
					const FSimEvent& E = H.Log[I];
					if (E.Type != EEvent::Form || E.Who != 0)
					{
						continue;
					}
					if (E.Kind == TEXT("borrowed"))
					{
						++Borrows;
						// (A tick's recovery may fall between the last reading and the payment.)
						TestTrue(FString::Printf(TEXT("borrowed with the price in hand (%.1f before)"), Prev), Prev >= G.QhiRules().ShapeshiftCost() - 0.1);
						TestTrue("and paid it", G.Qhi <= Prev - G.QhiRules().ShapeshiftCost() + 0.1);
						TestEqual("in the fight only", H.Sim.Phase, EPhase::Fight);
					}
					Breaks += E.Kind == TEXT("broken") ? 1 : 0;
					Reverts += E.Kind == TEXT("own") && H.Sim.Phase == EPhase::Fight ? 1 : 0;
				}
				Prev = G.Qhi;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d matches: %d shapeshifts, %d given back, %d broken"), Matches, Borrows, Reverts, Breaks));
	TestTrue("the Master CPU uses the shapeshift", Borrows >= Matches / 2);
	TestTrue("and mostly gives the shape back in time", Reverts >= Breaks);
	return true;
}

// Win rates of the two new fighters against everyone, by CPU difficulty (both sides at the same
// level, each side of the screen, several seeds), logged for tuning. CPU-against-CPU results swing
// a good deal with the seeds, so the bounds are wide: Wren an all-rounder, neither a pushover nor
// dominant; the Grandmaster weak in a novice CPU's hands (the meter misused) and holding his own
// at Master, where his secret fight is.
SLOP_TEST(FSlopNewMatchups, "AI.NewFightersMatchups")
bool FSlopNewMatchups::RunTest(const FString&)
{
	for (const TCHAR* Who : {TEXT("wren"), TEXT("grandmaster")})
	{
		for (int32 Level = 0; Level < (FName(Who) == FName(TEXT("grandmaster")) ? 4 : 3); ++Level)
		{
			int32 Won = 0, Played = 0;
			FString Line;
			for (const FName& Opp : FContent::Roster())
			{
				if (Opp == FName(Who))
				{
					continue;
				}
				int32 W = 0, N = 0;
				for (int32 Seed = 0; Seed < 8; ++Seed)
				{
					const int32 Side = Seed % 2;
					FFightSim Sim;
					Sim.bIntro = false;
					Sim.Setup(Side == 0 ? Fighter(Who) : GetContent().Fighter(Opp), Side == 0 ? GetContent().Fighter(Opp) : Fighter(Who));
					// Level 3 is the secret challenge: the Grandmaster's tier against Master opponents.
					const int32 OppLevel = FMath::Min(Level, 2);
					FCpuBrain C0(Sim, 0, Side == 0 ? Level : OppLevel, 1000 + Seed * 13 + Level);
					FCpuBrain C1(Sim, 1, Side == 0 ? OppLevel : Level, 2000 + Seed * 17 + Level);
					for (int32 T = 0; T < 60 * 60 * 6 && Sim.Phase != EPhase::MatchEnd; ++T)
					{
						FPadState Pads[2] = {C0.Think(), C1.Think()};
						Sim.Step(Pads);
					}
					if (Sim.MatchWinner >= 0)
					{
						++N;
						W += Sim.MatchWinner == Side ? 1 : 0;
					}
				}
				Won += W;
				Played += N;
				Line += FString::Printf(TEXT("%s %d/%d  "), *Opp.ToString(), W, N);
			}
			const double Rate = Played > 0 ? double(Won) / Played : 0.0;
			UE_LOG(LogTemp, Display, TEXT("Matchups %s level %d: %.0f%%  (%s)"), Who, Level, Rate * 100.0, *Line);
			if (FName(Who) == FName(TEXT("wren")))
			{
				TestTrue(FString::Printf(TEXT("Wren at level %d wins a fair share (%.0f%%)"), Level, Rate * 100), Rate >= 0.15 && Rate <= 0.8);
			}
			else if (Level == 2)
			{
				TestTrue(FString::Printf(TEXT("the Master CPU Grandmaster holds his own (%.0f%%)"), Rate * 100), Rate >= 0.3);
			}
			else if (Level == 3)
			{
				TestTrue(FString::Printf(TEXT("the challenge's Grandmaster beats Master CPUs more often than not (%.0f%%)"), Rate * 100), Rate > 0.5);
			}
		}
	}
	return true;
}

#undef SLOP_TEST
}

#endif
