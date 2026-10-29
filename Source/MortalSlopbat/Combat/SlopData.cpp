#include "Combat/SlopData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace Slop
{
	ELevel ParseLevel(const FString& S)
	{
		if (S == TEXT("high")) return ELevel::High;
		if (S == TEXT("mid")) return ELevel::Mid;
		if (S == TEXT("low")) return ELevel::Low;
		if (S == TEXT("overhead")) return ELevel::Overhead;
		if (S == TEXT("unblockable")) return ELevel::Unblockable;
		if (S == TEXT("throw")) return ELevel::Throw;
		return ELevel::None;
	}

	const TCHAR* LevelName(ELevel L)
	{
		switch (L)
		{
		case ELevel::High: return TEXT("High");
		case ELevel::Mid: return TEXT("Mid");
		case ELevel::Low: return TEXT("Low");
		case ELevel::Overhead: return TEXT("Overhead");
		case ELevel::Unblockable: return TEXT("Unblockable");
		case ELevel::Throw: return TEXT("Throw");
		default: return TEXT("");
		}
	}

	const TCHAR* StateName(EState S)
	{
		static const TCHAR* Names[] = {
			TEXT("INTRO"), TEXT("IDLE"), TEXT("WALK_F"), TEXT("WALK_B"), TEXT("CROUCH"), TEXT("RUN"), TEXT("BLOCK"),
			TEXT("CROUCH_BLOCK"), TEXT("PREJUMP"), TEXT("AIR"), TEXT("LAND"), TEXT("MOVE"), TEXT("BLOCKSTUN"),
			TEXT("HITSTUN"), TEXT("JUGGLE"), TEXT("FALL"), TEXT("DOWN"), TEXT("WAKEUP"), TEXT("THROWN"), TEXT("DIZZY"),
			TEXT("KO"), TEXT("WIN"), TEXT("FINISHER"), TEXT("VICTIM")};
		static_assert(UE_ARRAY_COUNT(Names) == (int32)EState::Count, "state names");
		return Names[(int32)S];
	}

	const TCHAR* EventName(EEvent E)
	{
		switch (E)
		{
		case EEvent::RoundStart: return TEXT("round_start");
		case EEvent::Fight: return TEXT("fight");
		case EEvent::Move: return TEXT("move");
		case EEvent::Chain: return TEXT("chain");
		case EEvent::Jump: return TEXT("jump");
		case EEvent::Land: return TEXT("land");
		case EEvent::Run: return TEXT("run");
		case EEvent::Hit: return TEXT("hit");
		case EEvent::Block: return TEXT("block");
		case EEvent::Armor: return TEXT("armor");
		case EEvent::Parry: return TEXT("parry");
		case EEvent::Throw: return TEXT("throw");
		case EEvent::ThrowBreak: return TEXT("throw_break");
		case EEvent::Projectile: return TEXT("projectile");
		case EEvent::ProjectileEnd: return TEXT("projectile_end");
		case EEvent::Clash: return TEXT("clash");
		case EEvent::Teleport: return TEXT("teleport");
		case EEvent::Buff: return TEXT("buff");
		case EEvent::BuffEnd: return TEXT("buff_end");
		case EEvent::ComboEnd: return TEXT("combo_end");
		case EEvent::KO: return TEXT("ko");
		case EEvent::FinishPrompt: return TEXT("finish_prompt");
		case EEvent::FinishTimeout: return TEXT("finish_timeout");
		case EEvent::FinisherRange: return TEXT("finisher_range");
		case EEvent::Finisher: return TEXT("finisher");
		case EEvent::FinisherEnd: return TEXT("finisher_end");
		case EEvent::Timeout: return TEXT("timeout");
		case EEvent::RoundEnd: return TEXT("round_end");
		case EEvent::WinPose: return TEXT("win_pose");
		case EEvent::MatchEnd: return TEXT("match_end");
		case EEvent::Line: return TEXT("line");
		case EEvent::StrangerEvade: return TEXT("stranger_evade");
		case EEvent::StrangerStrike: return TEXT("stranger_strike");
		case EEvent::StrangerCrack: return TEXT("stranger_crack");
		case EEvent::Form: return TEXT("form");
		}
		return TEXT("?");
	}

	namespace Json
	{
		double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default)
		{
			double V = Default;
			return (O.IsValid() && O->TryGetNumberField(Key, V)) ? V : Default;
		}
		int32 Int(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32 Default)
		{
			double V = 0;
			return (O.IsValid() && O->TryGetNumberField(Key, V)) ? (int32)V : Default;
		}
		bool Bool(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool Default)
		{
			bool V = Default;
			return (O.IsValid() && O->TryGetBoolField(Key, V)) ? V : Default;
		}
		FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FString& Default)
		{
			FString V;
			if (O.IsValid() && O->TryGetStringField(Key, V))
			{
				return V;
			}
			return Default;
		}
		FVector2D ArrVec2(const TArray<TSharedPtr<FJsonValue>>& A)
		{
			return A.Num() >= 2 ? FVector2D(A[0]->AsNumber(), A[1]->AsNumber()) : FVector2D::ZeroVector;
		}
		FVector2D Vec2(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FVector2D& Default)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = Arr(O, Key);
			return (A && A->Num() >= 2) ? ArrVec2(*A) : Default;
		}
		TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
		{
			const TSharedPtr<FJsonObject>* V = nullptr;
			return (O.IsValid() && O->TryGetObjectField(Key, V)) ? *V : nullptr;
		}
		const TArray<TSharedPtr<FJsonValue>>* Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
		{
			const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
			return (O.IsValid() && O->TryGetArrayField(Key, V)) ? V : nullptr;
		}
	}

	using namespace Json;

	int32 FMoveDef::HitAdvantage() const
	{
		if (Effect == TEXT("launch") || Effect == TEXT("knockdown") || Effect == TEXT("sweep") || Effect == TEXT("crumple"))
		{
			return 999;
		}
		return Hitstun - (Active + Recovery);
	}

	double FMoveDef::CurveAt(const TArray<FVector2D>& Curve, double Frame)
	{
		if (Curve.Num() == 0)
		{
			return 0.0;
		}
		if (Frame <= Curve[0].X)
		{
			return Curve[0].Y * FMath::Clamp(Frame / FMath::Max(Curve[0].X, 1.0), 0.0, 1.0);
		}
		for (int32 I = 1; I < Curve.Num(); ++I)
		{
			if (Frame <= Curve[I].X)
			{
				const double T = (Frame - Curve[I - 1].X) / FMath::Max(Curve[I].X - Curve[I - 1].X, 1e-5);
				return FMath::Lerp(Curve[I - 1].Y, Curve[I].Y, T);
			}
		}
		return Curve.Last().Y;
	}

	FString FFighterDef::NotationOf(FName MoveId) const
	{
		for (const FCommand& C : Commands)
		{
			if (C.Move == MoveId) return C.Text;
		}
		for (const FCommand& C : AirCommands)
		{
			if (C.Move == MoveId) return TEXT("Air ") + C.Text;
		}
		if (bHasFinisher && FinisherCmd.Move == MoveId)
		{
			return FinisherCmd.Text;
		}
		return FString();
	}

	static FIntPoint ParseRange(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = Arr(O, Key);
		if (A && A->Num() == 2)
		{
			return FIntPoint((int32)(*A)[0]->AsNumber(), (int32)(*A)[1]->AsNumber());
		}
		return FIntPoint::ZeroValue;
	}

	static EMoveKind ParseKind(const FString& S)
	{
		if (S == TEXT("special")) return EMoveKind::Special;
		if (S == TEXT("throw")) return EMoveKind::Throw;
		if (S == TEXT("grab")) return EMoveKind::Grab;
		if (S == TEXT("finisher")) return EMoveKind::Finisher;
		if (S == TEXT("taunt")) return EMoveKind::Taunt;
		if (S == TEXT("air")) return EMoveKind::Air;
		if (S == TEXT("chain")) return EMoveKind::Chain;
		return EMoveKind::Normal;
	}

	static void ParseKeys(const TSharedPtr<FJsonObject>& Anim, const TCHAR* Key, TArray<FAnimKey>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = Arr(Anim, Key);
		if (!A)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& K = V->AsArray();
			if (K.Num() < 2)
			{
				continue;
			}
			FAnimKey AK;
			AK.Frame = (int32)K[0]->AsNumber();
			AK.Pose = FName(*K[1]->AsString());
			AK.Ease = K.Num() > 2 ? FName(*K[2]->AsString()) : FName(TEXT("inout"));
			Out.Add(AK);
		}
	}

	static void ParseBoxes(const TSharedPtr<FJsonObject>& D, const TCHAR* Key, const FMoveDef& M, double Scale, TArray<FFrameBox>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = Arr(D, Key);
		if (!A)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> HB = V->AsObject();
			FFrameBox B;
			B.From = M.Startup;
			B.To = M.Startup + M.Active - 1;
			if (const TArray<TSharedPtr<FJsonValue>>* Fr = Arr(HB, TEXT("frames")))
			{
				if (Fr->Num() == 2)
				{
					B.From = (int32)(*Fr)[0]->AsNumber();
					B.To = (int32)(*Fr)[1]->AsNumber();
				}
			}
			if (const TArray<TSharedPtr<FJsonValue>>* R = Arr(HB, TEXT("rect")))
			{
				if (R->Num() == 4)
				{
					B.Rect = FRect((*R)[0]->AsNumber(), (*R)[1]->AsNumber(), (*R)[2]->AsNumber(), (*R)[3]->AsNumber()).Scaled(Scale);
				}
			}
			Out.Add(B);
		}
	}

	static void ParseCurve(const TSharedPtr<FJsonObject>& D, const TCHAR* Key, double Scale, TArray<FVector2D>& Out)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* A = Arr(D, Key))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FVector2D P = ArrVec2(V->AsArray());
				Out.Add(FVector2D(P.X, P.Y * Scale));
			}
		}
	}

	static TSharedRef<FMoveDef> ParseMove(FName Id, const TSharedPtr<FJsonObject>& D, double Scale, TArray<FString>& Errors, const FString& Owner)
	{
		TSharedRef<FMoveDef> MR = MakeShared<FMoveDef>();
		FMoveDef& M = MR.Get();
		M.Id = Id;
		M.Name = Str(D, TEXT("name"), Id.ToString());
		M.Kind = ParseKind(Str(D, TEXT("kind"), TEXT("normal")));
		M.Startup = Int(D, TEXT("startup"), 1);
		M.Active = Int(D, TEXT("active"), 0);
		M.Recovery = Int(D, TEXT("recovery"), 0);
		M.Total = FMath::Max(1, M.Startup - 1 + M.Active + M.Recovery);
		M.Total = Int(D, TEXT("total"), M.Total);
		const FString LevelS = Str(D, TEXT("level"), TEXT("none"));
		M.Level = ParseLevel(LevelS);
		if (M.Level == ELevel::None && LevelS != TEXT("none"))
		{
			Errors.Add(FString::Printf(TEXT("%s.%s: unknown level '%s'"), *Owner, *Id.ToString(), *LevelS));
		}
		M.Damage = Int(D, TEXT("damage"), 0);
		M.Chip = Int(D, TEXT("chip"), 0);
		M.Magic = FMath::Clamp(Num(D, TEXT("magic"), 0.0), 0.0, 1.0);
		M.QhiCost = FMath::Max(0.0, Num(D, TEXT("qhi"), 0.0));
		M.Hitstun = Int(D, TEXT("hitstun"), 18);
		M.Blockstun = Int(D, TEXT("blockstun"), 12);
		M.Hitstop = Int(D, TEXT("hitstop"), -1);
		M.Effect = FName(*Str(D, TEXT("effect"), TEXT("stagger")));
		M.Launch = Vec2(D, TEXT("launch"), FVector2D(1.5, 0.0));
		M.Juggle = Vec2(D, TEXT("juggle"), FVector2D(1.6, 4.2));
		M.JuggleCost = Int(D, TEXT("juggle_cost"), 1);
		M.PushHit = Num(D, TEXT("push_hit"), 0.35) * Scale;
		M.PushBlock = Num(D, TEXT("push_block"), 0.45) * Scale;
		ParseBoxes(D, TEXT("hitboxes"), M, Scale, M.Hitboxes);
		ParseBoxes(D, TEXT("hurtboxes"), M, Scale, M.Hurtboxes);
		ParseCurve(D, TEXT("motion"), Scale, M.Motion);
		ParseCurve(D, TEXT("lift"), Scale, M.Lift);
		if (const TArray<TSharedPtr<FJsonValue>>* Tr = Arr(D, TEXT("track")))
		{
			if (Tr->Num() == 3)
			{
				M.Track = FVector((*Tr)[0]->AsNumber(), (*Tr)[1]->AsNumber(), (*Tr)[2]->AsNumber());
			}
		}
		M.bAir = Bool(D, TEXT("air"), false);
		const TSharedPtr<FJsonObject> Props = Obj(D, TEXT("props"));
		M.Invuln = ParseRange(Props, TEXT("invuln"));
		M.ProjInvuln = ParseRange(Props, TEXT("proj_invuln"));
		M.Armor = ParseRange(Props, TEXT("armor"));
		M.LowProfile = ParseRange(Props, TEXT("low_profile"));
		M.Airborne = ParseRange(Props, TEXT("airborne"));
		M.Feint = ParseRange(Props, TEXT("feint"));
		M.Ward = ParseRange(Props, TEXT("ward"));
		M.bEndsCrouched = Bool(D, TEXT("ends_crouched"), false);
		if (const TArray<TSharedPtr<FJsonValue>>* Ch = Arr(D, TEXT("chain")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Ch)
			{
				const TSharedPtr<FJsonObject> C = V->AsObject();
				FChainLink L;
				L.Input = Str(C, TEXT("input"));
				FString Err;
				const FCommand Cmd = FCommand::Parse(L.Input, &Err);
				if (!Cmd.bValid)
				{
					Errors.Add(FString::Printf(TEXT("%s.%s chain: %s"), *Owner, *Id.ToString(), *Err));
				}
				L.Mask = Cmd.Mask;
				L.Steps = Cmd.Steps;
				L.Move = FName(*Str(C, TEXT("move")));
				const FVector2D W = Vec2(C, TEXT("window"), FVector2D(M.Startup, M.Total));
				L.From = (int32)W.X;
				L.To = (int32)W.Y;
				L.Cost = Num(C, TEXT("cost"), 12.0);
				M.Chain.Add(L);
			}
		}
		M.Cancel = ParseRange(D, TEXT("cancel"));
		M.SelfCancels = FMath::Max(0, Int(D, TEXT("self_cancels"), 0));
		if (const TSharedPtr<FJsonObject> P = Obj(D, TEXT("projectile")))
		{
			FProjectileDef& PD = M.Projectile;
			PD.bHas = true;
			PD.Scale = Scale;
			PD.Frame = Int(P, TEXT("frame"), M.Startup);
			PD.Kind = FName(*Str(P, TEXT("kind"), TEXT("orb")));
			PD.Offset = Vec2(P, TEXT("offset"), FVector2D(0.6, 1.3));
			if (const TArray<TSharedPtr<FJsonValue>>* AO = Arr(P, TEXT("at_opponent")))
			{
				PD.bAtOpponent = true;
				PD.AtOpponent = ArrVec2(*AO);
			}
			PD.Speed = Num(P, TEXT("speed"), 6.0);
			PD.Vy = Num(P, TEXT("vy"), 0.0);
			PD.Gravity = Num(P, TEXT("gravity"), 0.0);
			PD.Decel = Num(P, TEXT("decel"), 0.0);
			PD.MinSpeed = Num(P, TEXT("min_speed"), 0.0);
			PD.Size = Vec2(P, TEXT("size"), FVector2D(0.3, 0.3));
			PD.Life = Int(P, TEXT("life"), 90);
			PD.Hits = Int(P, TEXT("hits"), 1);
			PD.Strength = Int(P, TEXT("strength"), 1);
			PD.Level = ParseLevel(Str(P, TEXT("level"), TEXT("high")));
			PD.Damage = Int(P, TEXT("damage"), 60);
			PD.Chip = Int(P, TEXT("chip"), (int32)(PD.Damage * 0.2));
			PD.Hitstun = Int(P, TEXT("hitstun"), 24);
			PD.Blockstun = Int(P, TEXT("blockstun"), 14);
			PD.Hitstop = Int(P, TEXT("hitstop"), 8);
			PD.Effect = FName(*Str(P, TEXT("effect"), TEXT("stagger")));
			PD.Launch = Vec2(P, TEXT("launch"), FVector2D(1.5, 0.0));
			PD.Juggle = Vec2(P, TEXT("juggle"), FVector2D(1.8, 4.0));
			PD.PushHit = Num(P, TEXT("push_hit"), 0.5);
			PD.PushBlock = Num(P, TEXT("push_block"), 0.45);
			PD.Ground = FName(*Str(P, TEXT("ground"), TEXT("die")));
			PD.bReturns = Bool(P, TEXT("returns"), false);
			PD.bTether = Bool(P, TEXT("tether"), false);
			PD.Magic = FMath::Clamp(Num(P, TEXT("magic"), M.Magic), 0.0, 1.0);
		}
		if (const TSharedPtr<FJsonObject> T = Obj(D, TEXT("throw")))
		{
			FThrowDef& TD = M.Throw;
			TD.bHas = true;
			TD.Frames = Int(T, TEXT("frames"), 40);
			TD.DamageFrame = Int(T, TEXT("damage_frame"), 20);
			TD.Damage = Int(T, TEXT("damage"), 100);
			TD.Release = Vec2(T, TEXT("release"), FVector2D(2.0, 3.0));
			TD.Recover = Int(T, TEXT("recover"), 10);
			TD.bSwap = Bool(T, TEXT("swap"), false);
			TD.bBreakable = Bool(T, TEXT("breakable"), false);
			if (const TArray<TSharedPtr<FJsonValue>>* Path = Arr(T, TEXT("path")))
			{
				for (const TSharedPtr<FJsonValue>& V : *Path)
				{
					const TArray<TSharedPtr<FJsonValue>>& K = V->AsArray();
					if (K.Num() == 3)
					{
						TD.Path.Add(FVector(K[0]->AsNumber(), K[1]->AsNumber(), K[2]->AsNumber()));
					}
				}
			}
			if (TD.Path.Num() == 0)
			{
				TD.Path.Add(FVector(0, 0.7, 0.0));
			}
		}
		M.Requires = FName(*Str(D, TEXT("requires")));
		M.RunCost = Num(D, TEXT("run_cost"), 0.0);
		M.bFinisher = M.Kind == EMoveKind::Finisher;
		if (const TArray<TSharedPtr<FJsonValue>>* Tags = Arr(D, TEXT("tags")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Tags)
			{
				M.Tags.Add(FName(*V->AsString()));
			}
		}
		if (const TSharedPtr<FJsonObject> FX = Obj(D, TEXT("effects")))
		{
			M.ArmorHits = Int(FX, TEXT("armor_hits"), 1);
			M.HealthBelow = Num(FX, TEXT("health_below"), -1.0);
			M.Buff = FName(*Str(FX, TEXT("buff")));
			M.BuffFrame = Int(FX, TEXT("buff_frame"), M.Startup);
			M.BuffTicks = Int(FX, TEXT("buff_ticks"), 240);
			M.BuffHits = Int(FX, TEXT("buff_hits"), 2);
			if (const TSharedPtr<FJsonObject> Tp = Obj(FX, TEXT("teleport")))
			{
				M.bTeleport = true;
				M.TeleportFrame = Int(Tp, TEXT("frame"), 20);
				M.TeleportDist = Num(Tp, TEXT("dist"), 0.9);
			}
			if (const TArray<TSharedPtr<FJsonValue>>* AV = Arr(FX, TEXT("air_vel")))
			{
				if (AV->Num() == 3)
				{
					M.bAirVel = true;
					M.AirVel = FVector((*AV)[0]->AsNumber(), (*AV)[1]->AsNumber(), (*AV)[2]->AsNumber());
				}
			}
			if (const TSharedPtr<FJsonObject> Pr = Obj(FX, TEXT("parry")))
			{
				M.bParry = true;
				const FVector2D Fr = Vec2(Pr, TEXT("frames"), FVector2D(3, 20));
				M.ParryFrames = FIntPoint((int32)Fr.X, (int32)Fr.Y);
				M.ParryStun = Int(Pr, TEXT("stun"), 34);
				M.ParryCounter = FName(*Str(Pr, TEXT("counter")));
				M.ParryDamage = Int(Pr, TEXT("damage"), 0);
				M.bParryStagger = Bool(Pr, TEXT("stagger"), false);
				M.bParryCatch = Bool(Pr, TEXT("catch"), false);
				if (const TArray<TSharedPtr<FJsonValue>>* Lv = Arr(Pr, TEXT("levels")))
				{
					for (const TSharedPtr<FJsonValue>& V : *Lv)
					{
						M.ParryLevels.AddUnique(ParseLevel(V->AsString()));
					}
				}
			}
			M.bAirKnock = Bool(FX, TEXT("air_knock"), false);
			if (const TSharedPtr<FJsonObject> Sh = Obj(FX, TEXT("shapeshift")))
			{
				M.bShapeshift = true;
				M.FormFrame = Int(Sh, TEXT("frame"), M.Total);
			}
			if (const TSharedPtr<FJsonObject> Rv = Obj(FX, TEXT("revert")))
			{
				M.bRevert = true;
				M.FormFrame = Int(Rv, TEXT("frame"), M.Total);
			}
			M.StageInput = Str(FX, TEXT("input"), TEXT("D,D,D+LP"));
			M.StageId = FName(*Str(FX, TEXT("stage")));
			M.Sink = ParseRange(FX, TEXT("sink"));
			M.Whip = ParseRange(FX, TEXT("whip"));
		}
		if (const TSharedPtr<FJsonObject> Anim = Obj(D, TEXT("anim")))
		{
			ParseKeys(Anim, TEXT("keys"), M.AnimKeys);
			ParseKeys(Anim, TEXT("hold"), M.HoldKeys);
			ParseKeys(Anim, TEXT("victim"), M.VictimKeys);
			M.Tip = FName(*Str(Anim, TEXT("tip")));
		}
		M.Swing = FName(*Str(Obj(D, TEXT("sfx")), TEXT("swing")));
		M.HitSound = FName(*Str(Obj(D, TEXT("sfx")), TEXT("hit")));
		return MR;
	}

	// Parses [notation, move] pairs and orders them so the most specific input wins; ties keep
	// their data order.
	static TArray<FCommand> SortedCommands(const TArray<TPair<FString, FName>>& Pairs, TArray<FString>& Errors, const FString& Owner)
	{
		TArray<FCommand> Out;
		for (const TPair<FString, FName>& P : Pairs)
		{
			FString Err;
			FCommand C = FCommand::Parse(P.Key, &Err);
			if (!C.bValid)
			{
				Errors.Add(Owner + TEXT(": ") + Err);
				continue;
			}
			C.Move = P.Value;
			Out.Add(C);
		}
		Out.StableSort([](const FCommand& A, const FCommand& B) { return A.Priority() > B.Priority(); });
		return Out;
	}

	static void CollectPairs(const TSharedPtr<FJsonObject>& D, const TCHAR* Key, TArray<TPair<FString, FName>>& Out)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* A = Arr(D, Key))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
				if (P.Num() == 2)
				{
					Out.Emplace(P[0]->AsString(), FName(*P[1]->AsString()));
				}
			}
		}
	}

	const TArray<FName>& FContent::Roster()
	{
		static const TArray<FName> R = {TEXT("ashka"), TEXT("gorm"), TEXT("nix"), TEXT("mirela"), TEXT("bram"), TEXT("hollis"), TEXT("wren")};
		return R;
	}

	const TArray<FName>& FContent::Secret()
	{
		static const TArray<FName> S = {TEXT("grandmaster")};
		return S;
	}

	const TArray<FName>& FContent::Special()
	{
		static const TArray<FName> S = {TEXT("slopbat"), TEXT("daxion")};
		return S;
	}

	const TArray<FName>& FContent::Forms()
	{
		static const TArray<FName> S = {TEXT("grandmaster_spent")};
		return S;
	}

	TArray<FName> FContent::Fighting()
	{
		TArray<FName> Out = Roster();
		Out.Append(Secret());
		Out.Append(Forms());
		return Out;
	}

	TSharedPtr<FJsonObject> FContent::LoadJson(const FString& Path, TArray<FString>* Errors)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			if (Errors)
			{
				Errors->Add(TEXT("missing ") + Path);
			}
			return nullptr;
		}
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
		{
			if (Errors)
			{
				Errors->Add(TEXT("invalid JSON ") + Path + TEXT(": ") + Reader->GetErrorMessage());
			}
			return nullptr;
		}
		return Obj;
	}

	TSharedPtr<FJsonObject> FContent::ParseJson(const FString& Text)
	{
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid()) ? Obj : nullptr;
	}

	FContent& GetContent()
	{
		static FContent Content;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			Content.Load(FPaths::ProjectContentDir() / TEXT("Data"));
		}
		return Content;
	}

	bool FContent::Load(const FString& DataDir)
	{
		Dir = DataDir;
		Fighters.Reset();
		Errors.Reset();
		Basics = LoadJson(DataDir / TEXT("basics.json"), &Errors);
		CommonPosesOriginal = LoadJson(DataDir / TEXT("poses_common.json"), &Errors);
		CommonPoses = CommonPosesOriginal;
		// Refinements made since the port, laid over the original's data field by field: the shared
		// poses' style (poses_common_style.json) and the poses converted from motion capture
		// (poses_mocap.json, written by Tools/mocap), and the shared basics' animation
		// (basics_style.json). Each names the entries it refines; anything new is added.
		const auto Overlay = [](const TSharedPtr<FJsonObject>& Base, const TSharedPtr<FJsonObject>& Over)
		{
			TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->Values = Base->Values;
			for (const auto& KV : Over->Values)
			{
				TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>();
				if (const TSharedPtr<FJsonObject> Old = Obj(Base, *KV.Key))
				{
					Merged->Values = Old->Values;
				}
				if (const TSharedPtr<FJsonObject> Fields = KV.Value->AsObject())
				{
					for (const auto& FV : Fields->Values)
					{
						Merged->SetField(FString(*FV.Key), FV.Value);
					}
				}
				Out->SetObjectField(KV.Key, Merged);
			}
			return Out;
		};
		if (CommonPosesOriginal.IsValid())
		{
			for (const TCHAR* File : {TEXT("poses_common_style.json"), TEXT("poses_mocap.json")})
			{
				if (const TSharedPtr<FJsonObject> Over = LoadJson(DataDir / File))
				{
					CommonPoses = Overlay(CommonPoses, Over);
				}
			}
		}
		if (Basics.IsValid())
		{
			// basics_style.json is written by Tools/mocap; basics_tweaks.json is written by hand.
			for (const TCHAR* File : {TEXT("basics_style.json"), TEXT("basics_tweaks.json")})
			{
				const TSharedPtr<FJsonObject> Moves = Obj(Basics, TEXT("moves"));
				const TSharedPtr<FJsonObject> Over = LoadJson(DataDir / File);
				if (Moves.IsValid() && Over.IsValid())
				{
					Basics->SetObjectField(TEXT("moves"), Overlay(Moves, Over));
				}
			}
		}
		FightersMocap = LoadJson(DataDir / TEXT("fighters_mocap.json"));
		CyclesMocap = LoadJson(DataDir / TEXT("cycles_mocap.json"));
		Story = LoadJson(DataDir / TEXT("story.json"), &Errors);
		Trials = LoadJson(DataDir / TEXT("trials.json"), &Errors);
		if (Trials.IsValid())
		{
			Trials->RemoveField(TEXT("about"));
		}
		if (!Basics.IsValid())
		{
			return false;
		}
		TArray<FName> Ids = Roster();
		Ids.Append(Secret());
		Ids.Append(Special());
		Ids.Append(Forms());
		for (const FName& Id : Ids)
		{
			const FString Path = DataDir / TEXT("fighters") / (Id.ToString() + TEXT(".json"));
			TSharedPtr<FJsonObject> D = LoadJson(Path, &Errors);
			if (!D.IsValid())
			{
				continue;
			}
			// Appearance and own key poses live apart from how the fighter fights; a form of another
			// fighter wears that fighter's body and look.
			const FString BodyId = Str(D, TEXT("body"), Id.ToString());
			if (TSharedPtr<FJsonObject> Look = LoadJson(DataDir / TEXT("looks") / (BodyId + TEXT(".json"))))
			{
				D->SetObjectField(TEXT("look"), Look);
			}
			TSharedPtr<FJsonObject> Poses = LoadJson(DataDir / TEXT("poses") / (Id.ToString() + TEXT(".json")));
			if (!Poses.IsValid())
			{
				Poses = MakeShared<FJsonObject>();
			}
			if (const TSharedPtr<FJsonObject> Inline = Obj(D, TEXT("poses")))
			{
				for (const auto& KV : Inline->Values)
				{
					Poses->SetField(FString(*KV.Key), KV.Value);
				}
			}
			D->SetObjectField(TEXT("poses"), Poses);
			// Performances converted from motion capture (Tools/mocap): fields laid over the fighter.
			if (FightersMocap.IsValid())
			{
				if (const TSharedPtr<FJsonObject> Over = Obj(FightersMocap, *Id.ToString()))
				{
					for (const auto& KV : Over->Values)
					{
						D->SetField(KV.Key, KV.Value);
					}
				}
			}
			// Style refinements made after the port, kept apart from the original's own poses
			// (poses/<id>_style.json: fields laid over the poses they name).
			if (TSharedPtr<FJsonObject> Style = LoadJson(DataDir / TEXT("poses") / (Id.ToString() + TEXT("_style.json"))))
			{
				D->SetObjectField(TEXT("pose_style"), Style);
			}
			TSharedPtr<FFighterDef> F = ParseFighter(D);
			if (F.IsValid())
			{
				Validate(*F);
				Fighters.Add(Id, F.ToSharedRef());
			}
		}
		TArray<FName> Required = Roster();
		Required.Append(Secret());
		for (const FName& Id : Required)
		{
			if (!Fighters.Contains(Id))
			{
				Errors.Add(TEXT("roster fighter missing: ") + Id.ToString());
			}
		}
		for (auto& KV : Fighters)
		{
			FFighterDef& F = KV.Value.Get();
			if (!F.SpentFormId.IsNone())
			{
				F.SpentForm = Fighter(F.SpentFormId);
				if (!F.SpentForm || F.SpentForm->Resource == EResource::Qhi)
				{
					Errors.Add(KV.Key.ToString() + TEXT(": spent form missing or itself a Qhi fighter: ") + F.SpentFormId.ToString());
				}
			}
			else if (F.Resource == EResource::Qhi)
			{
				Errors.Add(KV.Key.ToString() + TEXT(": a Qhi fighter needs a spent form"));
			}
		}
		return Errors.Num() == 0;
	}

	TSharedPtr<FFighterDef> FContent::ParseFighter(const TSharedPtr<FJsonObject>& D)
	{
		TSharedPtr<FFighterDef> FP = MakeShared<FFighterDef>();
		FFighterDef& F = *FP;
		F.Raw = D;
		F.Id = FName(*Str(D, TEXT("id")));
		const FString Owner = F.Id.ToString();
		F.Name = Str(D, TEXT("name"), Owner);
		F.Title = Str(D, TEXT("title"));
		F.Pronoun = Str(D, TEXT("pronoun"), TEXT("him"));
		F.Style = Str(D, TEXT("style"));
		F.Blurb = Str(D, TEXT("blurb"));
		F.bBoss = Bool(D, TEXT("boss"), false);
		F.bHidden = Bool(D, TEXT("hidden"), F.bBoss);
		F.bCopyable = Bool(D, TEXT("copyable"), !F.bHidden);
		F.Body = FName(*Str(D, TEXT("body"), Owner));
		F.SpentFormId = FName(*Str(D, TEXT("spent_form")));
		const FString Res = Str(D, TEXT("resource"), TEXT("run"));
		F.Resource = Res == TEXT("qhi") ? EResource::Qhi : (Res == TEXT("none") ? EResource::None : EResource::Run);
		if (Res != TEXT("qhi") && Res != TEXT("none") && Res != TEXT("run"))
		{
			Errors.Add(Owner + TEXT(": unknown resource ") + Res);
		}
		if (const TSharedPtr<FJsonObject> Q = Obj(D, TEXT("qhi")))
		{
			FQhiRules& R = F.Qhi;
			R.Max = Num(Q, TEXT("max"), R.Max);
			R.Regen = Num(Q, TEXT("regen"), R.Regen);
			R.RegenDelay = Int(Q, TEXT("regen_delay"), R.RegenDelay);
			R.SpentRegen = Num(Q, TEXT("spent_regen"), R.SpentRegen);
			R.RecoverAt = Num(Q, TEXT("recover_at"), R.RecoverAt);
			R.GlideDrain = Num(Q, TEXT("glide_drain"), R.GlideDrain);
			R.GlideMin = Num(Q, TEXT("glide_min"), R.GlideMin);
			R.ChainShare = Num(Q, TEXT("chain_share"), R.ChainShare);
			R.Upkeep = Num(Q, TEXT("upkeep"), R.Upkeep);
			R.HitDrain = Num(Q, TEXT("hit_drain"), R.HitDrain);
			R.DefendGain = Num(Q, TEXT("defend_gain"), R.DefendGain);
			R.SpentDefendGain = Num(Q, TEXT("spent_defend_gain"), R.SpentDefendGain);
			R.DefendGainEvery = Int(Q, TEXT("defend_gain_every"), R.DefendGainEvery);
			R.FailCost = Num(Q, TEXT("fail_cost"), R.FailCost);
			R.CopyPower = Num(Q, TEXT("copy_power"), R.CopyPower);
			R.CopyGuard = Num(Q, TEXT("copy_guard"), R.CopyGuard);
			R.CopySpeed = Num(Q, TEXT("copy_speed"), R.CopySpeed);
		}
		if (const TSharedPtr<FJsonObject> Mo = Obj(D, TEXT("motion")))
		{
			FMotionStyle& M = F.Motion;
			M.Bounce = Num(Mo, TEXT("bounce"), M.Bounce);
			M.Rock = Num(Mo, TEXT("rock"), M.Rock);
			M.Pace = Num(Mo, TEXT("pace"), M.Pace);
			M.WalkBob = Num(Mo, TEXT("walk_bob"), M.WalkBob);
			M.StepLift = Num(Mo, TEXT("step_lift"), M.StepLift);
			M.WalkTwist = Num(Mo, TEXT("walk_twist"), M.WalkTwist);
			M.CircleWalk = Num(Mo, TEXT("circle_walk"), M.CircleWalk);
			M.bHeadLocked = Bool(Mo, TEXT("head_locked"), M.bHeadLocked);
			M.bWalkCycle = Bool(Mo, TEXT("walk_cycle"), M.bWalkCycle);
			M.bRunCycle = Bool(Mo, TEXT("run_cycle"), M.bRunCycle);
		}
		if (CyclesMocap.IsValid() && !F.bBoss)
		{
			for (const auto& KV : CyclesMocap->Values)
			{
				const FName Name(*KV.Key);
				const bool bRun = Name == FName(TEXT("run"));
				if ((bRun && !F.Motion.bRunCycle) || (!bRun && !F.Motion.bWalkCycle))
				{
					continue;
				}
				const TSharedPtr<FJsonObject> C = KV.Value->AsObject();
				FCycleDef Cycle;
				Cycle.Stride = Num(C, TEXT("stride"), 1.0);
				if (const TArray<TSharedPtr<FJsonValue>>* P = Arr(C, TEXT("poses")))
				{
					for (const TSharedPtr<FJsonValue>& V : *P)
					{
						Cycle.Poses.Add(FName(*V->AsString()));
					}
				}
				if (Cycle.Poses.Num() > 1 && Cycle.Stride > 0.05)
				{
					F.Cycles.Add(Name, Cycle);
				}
			}
		}
		const TSharedPtr<FJsonObject> S = Obj(D, TEXT("stats"));
		F.Height = Num(S, TEXT("height"), F.Height);
		F.Health = Int(S, TEXT("health"), F.Health);
		F.WalkF = Num(S, TEXT("walk_f"), F.WalkF);
		F.WalkB = Num(S, TEXT("walk_b"), F.WalkB);
		F.RunSpeed = Num(S, TEXT("run_speed"), F.RunSpeed);
		F.JumpVy = Num(S, TEXT("jump_vy"), F.JumpVy);
		F.JumpVx = Num(S, TEXT("jump_vx"), F.JumpVx);
		F.Gravity = Num(S, TEXT("gravity"), F.Gravity);
		const double K = F.Height / FFighterDef::BaseHeight;
		F.PushW = Num(S, TEXT("push_w"), F.PushW * K);
		F.ThrowRange = Num(S, TEXT("throw_range"), F.ThrowRange * K);
		F.bUnthrowable = Bool(S, TEXT("unthrowable"), false);
		F.PhysDef = Num(S, TEXT("phys_def"), F.PhysDef);
		F.MagicDef = Num(S, TEXT("magic_def"), F.MagicDef);
		F.Power = Num(S, TEXT("power"), F.Power);
		F.Arcana = Num(S, TEXT("arcana"), F.Arcana);

		const TSharedPtr<FJsonObject> BasicMoves = Obj(Basics, TEXT("moves"));
		TSet<FString> Skip;
		FString NoBasics;
		if (D->TryGetStringField(TEXT("no_basics"), NoBasics) && NoBasics == TEXT("all"))
		{
			if (BasicMoves.IsValid())
			{
				for (const auto& KV : BasicMoves->Values)
				{
					Skip.Add(FString(*KV.Key));
				}
			}
		}
		else if (const TArray<TSharedPtr<FJsonValue>>* NB = Arr(D, TEXT("no_basics")))
		{
			for (const TSharedPtr<FJsonValue>& V : *NB)
			{
				Skip.Add(V->AsString());
			}
		}
		const TSharedPtr<FJsonObject> Own = Obj(D, TEXT("moves"));
		const TSharedPtr<FJsonObject> Tweaks = Obj(D, TEXT("basic_tweaks"));
		if (BasicMoves.IsValid())
		{
			for (const auto& KV : BasicMoves->Values)
			{
				if (Skip.Contains(FString(*KV.Key)) || (Own.IsValid() && Own->HasField(FString(*KV.Key))))
				{
					continue;
				}
				TSharedPtr<FJsonObject> Src = KV.Value->AsObject();
				if (const TSharedPtr<FJsonObject> Tw = Obj(Tweaks, *KV.Key))
				{
					// A tweak replaces whole top-level keys of the shared move.
					TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>();
					Merged->Values = Src->Values;
					for (const auto& TV : Tw->Values)
					{
						Merged->SetField(FString(*TV.Key), TV.Value);
					}
					Src = Merged;
				}
				F.Moves.Add(FName(*KV.Key), ParseMove(FName(*KV.Key), Src, K, Errors, Owner));
			}
		}
		if (Own.IsValid())
		{
			for (const auto& KV : Own->Values)
			{
				F.Moves.Add(FName(*KV.Key), ParseMove(FName(*KV.Key), KV.Value->AsObject(), 1.0, Errors, Owner));
			}
		}
		auto Gather = [&](const TCHAR* Key)
		{
			TArray<TPair<FString, FName>> Pairs;
			CollectPairs(D, Key, Pairs);
			TArray<TPair<FString, FName>> Shared;
			CollectPairs(Basics, Key, Shared);
			for (const TPair<FString, FName>& P : Shared)
			{
				if (F.Moves.Contains(P.Value) && !Skip.Contains(P.Value.ToString()))
				{
					Pairs.Add(P);
				}
			}
			return SortedCommands(Pairs, Errors, Owner);
		};
		F.Commands = Gather(TEXT("commands"));
		F.AirCommands = Gather(TEXT("air_commands"));
		if (const TSharedPtr<FJsonObject> Fin = Obj(D, TEXT("finisher")))
		{
			FString Err;
			F.FinisherCmd = FCommand::Parse(Str(Fin, TEXT("input")), &Err);
			if (!F.FinisherCmd.bValid)
			{
				Errors.Add(Owner + TEXT(" finisher: ") + Err);
			}
			F.FinisherCmd.Move = FName(*Str(Fin, TEXT("move")));
			F.FinisherRange = FName(*Str(Fin, TEXT("range"), TEXT("close")));
			F.bHasFinisher = F.FinisherCmd.bValid;
		}
		F.Poses = Obj(D, TEXT("poses"));
		if (const TSharedPtr<FJsonObject> States = Obj(D, TEXT("state_anim")))
		{
			for (const auto& KV : States->Values)
			{
				ParseKeys(States, *KV.Key, F.StateAnims.Add(FName(*KV.Key)));
			}
		}
		F.PoseStyle = Obj(D, TEXT("pose_style"));
		F.Look = Obj(D, TEXT("look"));
		F.AI = Obj(D, TEXT("ai"));
		return FP;
	}

	void FContent::Validate(const FFighterDef& F)
	{
		const FString Owner = F.Id.ToString();
		auto PoseExists = [&](FName Pose)
		{
			const FString P = Pose.ToString();
			return (F.Poses.IsValid() && F.Poses->HasField(P)) || (F.PoseStyle.IsValid() && F.PoseStyle->HasField(P)) || (CommonPoses.IsValid() && CommonPoses->HasField(P));
		};
		for (const FCommand& C : F.Commands)
		{
			if (!F.Move(C.Move))
			{
				Errors.Add(FString::Printf(TEXT("%s: command %s names unknown move %s"), *Owner, *C.Text, *C.Move.ToString()));
			}
		}
		for (const FCommand& C : F.AirCommands)
		{
			const FMoveDef* M = F.Move(C.Move);
			if (!M)
			{
				Errors.Add(FString::Printf(TEXT("%s: air command %s names unknown move %s"), *Owner, *C.Text, *C.Move.ToString()));
			}
			else if (!M->bAir)
			{
				Errors.Add(FString::Printf(TEXT("%s: air command %s starts ground move %s"), *Owner, *C.Text, *C.Move.ToString()));
			}
		}
		if (F.bHasFinisher)
		{
			const FMoveDef* M = F.Move(F.FinisherCmd.Move);
			if (!M || !M->bFinisher)
			{
				Errors.Add(FString::Printf(TEXT("%s: finisher %s is not a finisher move"), *Owner, *F.FinisherCmd.Move.ToString()));
			}
		}
		for (const auto& KV : F.Moves)
		{
			const FMoveDef& M = KV.Value.Get();
			const FString Where = Owner + TEXT(".") + M.Id.ToString();
			if (M.Startup < 1 || M.Active < 0 || M.Recovery < 0 || M.Total < 1)
			{
				Errors.Add(Where + TEXT(": frame counts out of range"));
			}
			for (const FFrameBox& B : M.Hitboxes)
			{
				if (B.From < 1 || B.To < B.From || B.To > M.Total || B.Rect.W <= 0 || B.Rect.H <= 0)
				{
					Errors.Add(Where + TEXT(": hitbox frames or size out of range"));
				}
			}
			if (M.IsAttack() && M.Level != ELevel::Throw && M.Hitboxes.Num() == 0 && !M.Projectile.bHas && M.Kind != EMoveKind::Grab)
			{
				Errors.Add(Where + TEXT(": attack without hitboxes"));
			}
			for (const FChainLink& L : M.Chain)
			{
				if (!F.Move(L.Move))
				{
					Errors.Add(Where + TEXT(": chain to unknown move ") + L.Move.ToString());
				}
			}
			if (M.bParry && !M.ParryCounter.IsNone() && !F.Move(M.ParryCounter))
			{
				Errors.Add(Where + TEXT(": parry counter is unknown"));
			}
			if (M.bParryCatch && (!F.Move(M.ParryCounter) || !F.Move(M.ParryCounter)->Throw.bHas))
			{
				Errors.Add(Where + TEXT(": a catching parry needs a counter with a throw sequence"));
			}
			if (M.IsThrowKind() && M.Kind == EMoveKind::Throw && !M.Throw.bHas)
			{
				Errors.Add(Where + TEXT(": throw without a throw sequence"));
			}
			if (M.QhiCost > 0.0 && F.Resource != EResource::Qhi)
			{
				Errors.Add(Where + TEXT(": costs Qhi, but the fighter has no Qhi meter"));
			}
			if ((M.bShapeshift || M.bRevert) && (M.FormFrame < 1 || M.FormFrame > M.Total || M.IsAttack()))
			{
				Errors.Add(Where + TEXT(": a change of shape needs a frame within the move and no attack"));
			}
			for (const TArray<FAnimKey>* Keys : {&M.AnimKeys, &M.HoldKeys, &M.VictimKeys})
			{
				int32 Prev = -1;
				for (const FAnimKey& AK : *Keys)
				{
					if (!PoseExists(AK.Pose))
					{
						Errors.Add(FString::Printf(TEXT("%s: animation names unknown pose %s"), *Where, *AK.Pose.ToString()));
					}
					if (AK.Frame < Prev)
					{
						Errors.Add(Where + TEXT(": animation keys out of order"));
					}
					Prev = AK.Frame;
				}
			}
		}
	}

	TSharedRef<FFighterDef> MakeBorrowedShape(const FFighterDef& Target, const FFighterDef& Copier)
	{
		TSharedRef<FFighterDef> S = MakeShared<FFighterDef>(Target);
		const FQhiRules& Q = Copier.Qhi;
		S->CopyOf = &Target;
		S->Resource = EResource::Run;
		S->bCopyable = false;
		S->SpentForm = nullptr;
		S->SpentFormId = NAME_None;
		S->Power = Target.Power * Q.CopyPower;
		S->Arcana = Target.Arcana * Q.CopyPower;
		S->PhysDef = Target.PhysDef * Q.CopyGuard;
		S->MagicDef = Target.MagicDef * Q.CopyGuard;
		S->WalkF = Target.WalkF * Q.CopySpeed;
		S->WalkB = Target.WalkB * Q.CopySpeed;
		S->RunSpeed = Target.RunSpeed * Q.CopySpeed;
		// The fight's end lets the shape go, so it never performs the other fighter's finisher.
		S->bHasFinisher = false;
		// The way back: the copier's reversal, on the copier's own input for it, ahead of any of
		// the target's commands on the same input.
		for (const FCommand& C : Copier.Commands)
		{
			const TSharedRef<FMoveDef>* M = Copier.Moves.Find(C.Move);
			if (M && (*M)->bRevert)
			{
				S->Moves.Add(C.Move, *M);
				S->Commands.Insert(C, 0);
			}
		}
		S->Commands.StableSort([](const FCommand& A, const FCommand& B) { return A.Priority() > B.Priority(); });
		return S;
	}
}
