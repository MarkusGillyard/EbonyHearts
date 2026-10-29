#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/SlopSim.h"
#include "Combat/SlopInput.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Writes every fighter's definition as the engine holds it after loading (shared basics laid in,
// moves scaled to the fighter's height, the Grandmaster's form library expanded) to
// Saved/Reports/CombatData.json, with the simulation's own constants: the data a handoff or a
// balance sheet should quote, never re-derived by hand from the JSON files.
namespace SlopExportTestSuite
{
using namespace Slop;

const TCHAR* KindName(EMoveKind K)
{
	switch (K)
	{
	case EMoveKind::Normal: return TEXT("normal");
	case EMoveKind::Special: return TEXT("special");
	case EMoveKind::Throw: return TEXT("throw");
	case EMoveKind::Grab: return TEXT("grab");
	case EMoveKind::Finisher: return TEXT("finisher");
	case EMoveKind::Taunt: return TEXT("taunt");
	case EMoveKind::Air: return TEXT("air");
	case EMoveKind::Chain: return TEXT("chain");
	}
	return TEXT("?");
}

TArray<TSharedPtr<FJsonValue>> Pair(const FIntPoint& P) { return {MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y)}; }
TArray<TSharedPtr<FJsonValue>> Pair(const FVector2D& P) { return {MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y)}; }

TSharedRef<FJsonObject> MoveJson(const FFighterDef& F, const FMoveDef& M)
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), M.Id.ToString());
	O->SetStringField(TEXT("name"), M.Name);
	O->SetStringField(TEXT("kind"), KindName(M.Kind));
	TArray<TSharedPtr<FJsonValue>> Inputs;
	for (const TArray<FCommand>* List : {&F.Commands, &F.AirCommands})
	{
		for (const FCommand& C : *List)
		{
			if (C.Move == M.Id)
			{
				Inputs.Add(MakeShared<FJsonValueString>((List == &F.AirCommands ? TEXT("air ") : TEXT("")) + C.Text));
			}
		}
	}
	O->SetArrayField(TEXT("inputs"), Inputs);
	// How the move is reached when no command starts it: a chain link or a parry's counter.
	TArray<TSharedPtr<FJsonValue>> From;
	for (const TPair<FName, TSharedRef<FMoveDef>>& KV : F.Moves)
	{
		for (const FChainLink& L : KV.Value->Chain)
		{
			if (L.Move == M.Id)
			{
				From.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("chain from %s (%s)"), *KV.Key.ToString(), *L.Input)));
			}
		}
		if (KV.Value->ParryCounter == M.Id)
		{
			From.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("counter of %s"), *KV.Key.ToString())));
		}
	}
	O->SetArrayField(TEXT("reached_from"), From);
	O->SetStringField(TEXT("level"), LevelName(M.Level));
	O->SetNumberField(TEXT("damage"), M.Damage);
	O->SetNumberField(TEXT("chip"), M.Chip);
	O->SetNumberField(TEXT("startup"), M.Startup);
	O->SetNumberField(TEXT("active"), M.Active);
	O->SetNumberField(TEXT("recovery"), M.Recovery);
	O->SetNumberField(TEXT("total"), M.Total);
	O->SetNumberField(TEXT("hitstun"), M.Hitstun);
	O->SetNumberField(TEXT("blockstun"), M.Blockstun);
	O->SetNumberField(TEXT("hitstop"), M.Hitstop);
	if (M.IsAttack() && M.Active > 0)
	{
		O->SetNumberField(TEXT("on_block"), M.BlockAdvantage());
		const int32 Hit = M.HitAdvantage();
		O->SetField(TEXT("on_hit"), Hit >= 999 ? StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueString>(TEXT("knockdown"))) : StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueNumber>(Hit)));
	}
	O->SetStringField(TEXT("effect"), M.Effect.ToString());
	O->SetArrayField(TEXT("launch"), Pair(M.Launch));
	O->SetNumberField(TEXT("push_hit"), M.PushHit);
	O->SetNumberField(TEXT("push_block"), M.PushBlock);
	O->SetNumberField(TEXT("magic_share"), M.Magic);
	O->SetNumberField(TEXT("hitboxes"), M.Hitboxes.Num());
	if (M.Hitboxes.Num() > 0)
	{
		double Reach = 0.0;
		for (const FFrameBox& B : M.Hitboxes)
		{
			Reach = FMath::Max(Reach, B.Rect.EndX());
		}
		O->SetNumberField(TEXT("hitbox_reach_m"), FMath::RoundToDouble(Reach * 1000.0) / 1000.0);
	}
	if (M.bAir) O->SetBoolField(TEXT("air"), true);
	if (M.Cancel != FIntPoint::ZeroValue) O->SetArrayField(TEXT("cancel_window"), Pair(M.Cancel));
	if (M.SelfCancels > 0) O->SetNumberField(TEXT("self_cancels"), M.SelfCancels);
	if (M.Chain.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Links;
		for (const FChainLink& L : M.Chain)
		{
			TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
			J->SetStringField(TEXT("input"), L.Input);
			J->SetStringField(TEXT("move"), L.Move.ToString());
			J->SetArrayField(TEXT("window"), Pair(FIntPoint(L.From, L.To)));
			J->SetNumberField(TEXT("run_cost"), L.Cost);
			Links.Add(MakeShared<FJsonValueObject>(J));
		}
		O->SetArrayField(TEXT("chain"), Links);
	}
	if (M.RunCost > 0.0) O->SetNumberField(TEXT("run_cost"), M.RunCost);
	if (M.QhiCost > 0.0) O->SetNumberField(TEXT("qhi_cost"), M.QhiCost);
	if (M.bQhiAll) O->SetBoolField(TEXT("qhi_all"), true);
	if (M.Invuln != FIntPoint::ZeroValue) O->SetArrayField(TEXT("invulnerable"), Pair(M.Invuln));
	if (M.ProjInvuln != FIntPoint::ZeroValue) O->SetArrayField(TEXT("projectile_invulnerable"), Pair(M.ProjInvuln));
	if (M.Armor != FIntPoint::ZeroValue) { O->SetArrayField(TEXT("armor"), Pair(M.Armor)); O->SetNumberField(TEXT("armor_hits"), M.ArmorHits); }
	if (M.LowProfile != FIntPoint::ZeroValue) O->SetArrayField(TEXT("low_profile"), Pair(M.LowProfile));
	if (M.Airborne != FIntPoint::ZeroValue) O->SetArrayField(TEXT("airborne"), Pair(M.Airborne));
	if (!M.Requires.IsNone()) O->SetStringField(TEXT("requires"), M.Requires.ToString());
	if (M.HealthBelow >= 0.0) O->SetNumberField(TEXT("health_below"), M.HealthBelow);
	if (!M.Buff.IsNone()) { O->SetStringField(TEXT("buff"), M.Buff.ToString()); O->SetNumberField(TEXT("buff_ticks"), M.BuffTicks); }
	if (M.bTeleport) { O->SetNumberField(TEXT("teleport_frame"), M.TeleportFrame); O->SetNumberField(TEXT("teleport_m"), M.TeleportDist); }
	if (M.Feint != FIntPoint::ZeroValue) O->SetArrayField(TEXT("feint_cancel"), Pair(M.Feint));
	if (M.Ward != FIntPoint::ZeroValue) O->SetArrayField(TEXT("ward"), Pair(M.Ward));
	if (M.bParry)
	{
		TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetArrayField(TEXT("frames"), Pair(M.ParryFrames));
		TArray<TSharedPtr<FJsonValue>> Lv;
		for (ELevel L : {ELevel::High, ELevel::Mid, ELevel::Low, ELevel::Overhead})
		{
			if (M.Parries(L))
			{
				Lv.Add(MakeShared<FJsonValueString>(LevelName(L)));
			}
		}
		P->SetArrayField(TEXT("catches"), Lv);
		P->SetNumberField(TEXT("stun"), M.ParryStun);
		if (!M.ParryCounter.IsNone()) P->SetStringField(TEXT("counter"), M.ParryCounter.ToString());
		if (M.ParryDamage > 0) P->SetNumberField(TEXT("sting"), M.ParryDamage);
		if (M.bParryStagger) P->SetBoolField(TEXT("stagger"), true);
		if (M.bParryCatch) P->SetBoolField(TEXT("catch_into_hold"), true);
		O->SetObjectField(TEXT("parry"), P);
	}
	if (M.Projectile.bHas)
	{
		const FProjectileDef& P = M.Projectile;
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("kind"), P.Kind.ToString());
		J->SetNumberField(TEXT("frame"), P.Frame);
		J->SetStringField(TEXT("level"), LevelName(P.Level));
		J->SetNumberField(TEXT("damage"), P.Damage);
		J->SetNumberField(TEXT("chip"), P.Chip);
		J->SetNumberField(TEXT("speed_mps"), P.Speed);
		J->SetNumberField(TEXT("life_ticks"), P.Life);
		J->SetNumberField(TEXT("hits"), P.Hits);
		J->SetNumberField(TEXT("hitstun"), P.Hitstun);
		J->SetNumberField(TEXT("blockstun"), P.Blockstun);
		J->SetStringField(TEXT("effect"), P.Effect.ToString());
		J->SetNumberField(TEXT("magic_share"), P.Magic);
		if (P.bReturns) J->SetBoolField(TEXT("returns"), true);
		if (P.bTether) J->SetBoolField(TEXT("tether"), true);
		O->SetObjectField(TEXT("projectile"), J);
	}
	if (M.Throw.bHas)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("frames"), M.Throw.Frames);
		J->SetNumberField(TEXT("damage_frame"), M.Throw.DamageFrame);
		J->SetNumberField(TEXT("damage"), M.Throw.Damage);
		// A throw can always be broken in its first ThrowBreakWindow ticks; a grab only when it says so.
		J->SetBoolField(TEXT("breakable"), M.Kind == EMoveKind::Throw || M.Throw.bBreakable);
		J->SetBoolField(TEXT("swaps_sides"), M.Throw.bSwap);
		O->SetObjectField(TEXT("throw"), J);
	}
	if (M.bShapeshift)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("target"), M.FormTarget.IsNone() ? TEXT("(the opponent facing him)") : M.FormTarget.ToString());
		J->SetNumberField(TEXT("qhi_cost"), M.FormTarget.IsNone() || M.FormCost < 0.0 ? F.Qhi.ShapeshiftCost() : M.FormCost);
		J->SetNumberField(TEXT("form_frame"), M.FormFrame);
		J->SetNumberField(TEXT("form_frame_in_combo"), F.Qhi.ShiftFrame);
		J->SetBoolField(TEXT("qhi_enhanced"), M.bFormEnhanced);
		J->SetNumberField(TEXT("upkeep_scale"), M.FormUpkeep);
		O->SetObjectField(TEXT("shapeshift"), J);
	}
	if (M.bRevert)
	{
		O->SetNumberField(TEXT("revert_frame"), M.FormFrame);
	}
	if (M.bKill)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("requires"), M.KillRequires.IsNone() ? TEXT("(none: reached only through its catch)") : M.KillRequires.ToString());
		if (M.KillRequires == TEXT("combo")) J->SetNumberField(TEXT("min_combo_hits"), M.KillCombo);
		O->SetObjectField(TEXT("instant_kill"), J);
	}
	if (M.bAnyShape) O->SetBoolField(TEXT("kept_in_every_shape"), true);
	if (M.bFinisher || M.Kind == EMoveKind::Finisher) O->SetBoolField(TEXT("finisher"), true);
	if (!M.StageId.IsNone()) O->SetStringField(TEXT("stage"), M.StageId.ToString());
	TArray<TSharedPtr<FJsonValue>> Tags;
	for (const FName& T : M.Tags)
	{
		Tags.Add(MakeShared<FJsonValueString>(T.ToString()));
	}
	if (Tags.Num() > 0) O->SetArrayField(TEXT("tags"), Tags);
	return O;
}

TSharedRef<FJsonObject> FighterJson(const FFighterDef& F, const TCHAR* Group)
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), F.Id.ToString());
	O->SetStringField(TEXT("name"), F.Name);
	O->SetStringField(TEXT("title"), F.Title);
	O->SetStringField(TEXT("style"), F.Style);
	O->SetStringField(TEXT("pronoun"), F.Pronoun);
	O->SetStringField(TEXT("group"), Group);
	O->SetStringField(TEXT("body"), F.Body.ToString());
	O->SetBoolField(TEXT("boss"), F.bBoss);
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("health"), F.Health);
	S->SetNumberField(TEXT("height_m"), F.Height);
	S->SetNumberField(TEXT("walk_forward_mps"), F.WalkF);
	S->SetNumberField(TEXT("walk_back_mps"), F.WalkB);
	S->SetNumberField(TEXT("run_mps"), F.RunSpeed);
	S->SetNumberField(TEXT("jump_vy"), F.JumpVy);
	S->SetNumberField(TEXT("jump_vx"), F.JumpVx);
	S->SetNumberField(TEXT("gravity"), F.Gravity);
	S->SetNumberField(TEXT("push_width_m"), F.PushW);
	S->SetNumberField(TEXT("throw_range_m"), F.ThrowRange);
	S->SetBoolField(TEXT("unthrowable"), F.bUnthrowable);
	S->SetNumberField(TEXT("physical_defense_taken_multiplier"), F.PhysDef);
	S->SetNumberField(TEXT("magic_defense_taken_multiplier"), F.MagicDef);
	S->SetNumberField(TEXT("physical_power"), F.Power);
	S->SetNumberField(TEXT("magic_power_arcana"), F.Arcana);
	S->SetStringField(TEXT("resource"), F.Resource == EResource::Qhi ? TEXT("qhi") : F.Resource == EResource::Run ? TEXT("run") : TEXT("none"));
	S->SetBoolField(TEXT("copyable_by_grandmaster"), F.bCopyable);
	S->SetNumberField(TEXT("shape_upkeep_scale"), F.ShapeUpkeep);
	O->SetObjectField(TEXT("stats"), S);
	if (F.Resource == EResource::Qhi)
	{
		const FQhiRules& Q = F.Qhi;
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("max"), Q.Max);
		J->SetNumberField(TEXT("regen_per_tick"), Q.Regen);
		J->SetNumberField(TEXT("regen_delay_ticks"), Q.RegenDelay);
		J->SetNumberField(TEXT("spent_regen_per_tick"), Q.SpentRegen);
		J->SetNumberField(TEXT("recover_at"), Q.RecoverAt);
		J->SetNumberField(TEXT("glide_drain_per_tick"), Q.GlideDrain);
		J->SetNumberField(TEXT("glide_min"), Q.GlideMin);
		J->SetNumberField(TEXT("chain_share_of_run_cost"), Q.ChainShare);
		J->SetNumberField(TEXT("shape_upkeep_per_tick"), Q.Upkeep);
		J->SetNumberField(TEXT("hit_drain_per_damage"), Q.HitDrain);
		J->SetNumberField(TEXT("defend_gain"), Q.DefendGain);
		J->SetNumberField(TEXT("spent_defend_gain"), Q.SpentDefendGain);
		J->SetNumberField(TEXT("defend_gain_every_ticks"), Q.DefendGainEvery);
		J->SetNumberField(TEXT("failed_shift_cost"), Q.FailCost);
		J->SetNumberField(TEXT("copy_power"), Q.CopyPower);
		J->SetNumberField(TEXT("copy_guard"), Q.CopyGuard);
		J->SetNumberField(TEXT("copy_speed"), Q.CopySpeed);
		J->SetNumberField(TEXT("opponent_copy_cost"), Q.ShapeshiftCost());
		J->SetNumberField(TEXT("shift_frame_in_combo"), Q.ShiftFrame);
		J->SetNumberField(TEXT("max_shifts_per_combo"), Q.MaxShifts);
		O->SetObjectField(TEXT("qhi"), J);
	}
	if (F.SpentForm) O->SetStringField(TEXT("spent_form"), F.SpentForm->Id.ToString());
	if (F.bHasFinisher)
	{
		O->SetStringField(TEXT("finisher_input"), F.FinisherCmd.Text);
		O->SetStringField(TEXT("finisher_move"), F.FinisherCmd.Move.ToString());
		O->SetStringField(TEXT("finisher_range"), F.FinisherRange.ToString());
	}
	TArray<FName> Ids;
	F.Moves.GetKeys(Ids);
	Ids.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	TArray<TSharedPtr<FJsonValue>> Moves;
	for (const FName& Id : Ids)
	{
		Moves.Add(MakeShared<FJsonValueObject>(MoveJson(F, *F.Move(Id))));
	}
	O->SetArrayField(TEXT("moves"), Moves);
	return O;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlopExportCombatData, "Slop.Content.ExportCombatData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSlopExportCombatData::RunTest(const FString&)
{
	const FContent& C = GetContent();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> K = MakeShared<FJsonObject>();
	K->SetNumberField(TEXT("ticks_per_second"), 60);
	K->SetNumberField(TEXT("round_ticks"), FFightSim::RoundTicks);
	K->SetNumberField(TEXT("finish_window_ticks"), FFightSim::FinishTicks);
	K->SetNumberField(TEXT("throw_break_window_ticks"), FFightSim::ThrowBreakWindow);
	K->SetNumberField(TEXT("stage_half_width_m"), FFightSim::StageHalf);
	K->SetNumberField(TEXT("max_separation_m"), FFightSim::MaxSep);
	K->SetNumberField(TEXT("start_gap_m"), FFightSim::StartGap);
	K->SetNumberField(TEXT("input_buffer_ticks"), FInputBuffer::BufferTicks);
	K->SetNumberField(TEXT("chord_ticks"), FInputBuffer::ChordTicks);
	K->SetNumberField(TEXT("motion_window_ticks"), FCommand::MotionWindow);
	K->SetNumberField(TEXT("run_meter_max"), FFighter::RunMax);
	K->SetNumberField(TEXT("run_drain_per_tick"), FFighter::RunDrain);
	K->SetNumberField(TEXT("run_regen_per_tick"), FFighter::RunRegen);
	K->SetNumberField(TEXT("run_regen_delay_ticks"), FFighter::RunRegenDelay);
	K->SetNumberField(TEXT("run_min_start"), FFighter::RunMinStart);
	K->SetNumberField(TEXT("juggle_limit_points"), FFighter::JuggleLimit);
	K->SetNumberField(TEXT("down_frames"), FFighter::DownFrames);
	K->SetNumberField(TEXT("wakeup_frames"), FFighter::WakeupFrames);
	K->SetStringField(TEXT("combo_damage_scaling"), TEXT("hit N of a combo deals damage x K, K = 1 for N <= 2, else max(0.4, 1 - 0.1 * (N - 2)); never below 1 point (SlopFightSim.cpp)"));
	K->SetStringField(TEXT("damage_formula"), TEXT("dealt = damage x ((1 - m) x attacker power x defender physical multiplier + m x attacker arcana x defender magic multiplier), m = the blow's magic share (Dealt, SlopFightSim.cpp)"));
	Root->SetObjectField(TEXT("engine_constants"), K);
	TArray<TSharedPtr<FJsonValue>> Fighters;
	auto AddGroup = [&](const TArray<FName>& Ids, const TCHAR* Group)
	{
		for (const FName& Id : Ids)
		{
			if (const FFighterDef* F = C.Fighter(Id))
			{
				Fighters.Add(MakeShared<FJsonValueObject>(FighterJson(*F, Group)));
			}
		}
	};
	AddGroup(FContent::Roster(), TEXT("starting roster"));
	AddGroup(FContent::Secret(), TEXT("secret (unlocked)"));
	AddGroup(FContent::Special(), TEXT("final boss"));
	AddGroup(FContent::Forms(), TEXT("form (the Grandmaster with no Qhi)"));
	Root->SetArrayField(TEXT("fighters"), Fighters);
	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, W);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Reports/CombatData.json");
	TestTrue("written", FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestEqual("every fighter", Fighters.Num(), FContent::Roster().Num() + FContent::Secret().Num() + FContent::Special().Num() + FContent::Forms().Num());
	AddInfo(Path);
	return true;
}
}

#endif
