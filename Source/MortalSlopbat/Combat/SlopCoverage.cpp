#include "Combat/SlopCoverage.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

namespace Slop
{
	TSet<FName> ReachableMoves(const FFighterDef& F)
	{
		TSet<FName> Seen;
		TArray<FName> Open;
		auto Add = [&](FName Id)
		{
			if (!Id.IsNone() && F.Move(Id) && !Seen.Contains(Id))
			{
				Seen.Add(Id);
				Open.Add(Id);
			}
		};
		for (const FCommand& C : F.Commands)
		{
			Add(C.Move);
		}
		for (const FCommand& C : F.AirCommands)
		{
			Add(C.Move);
		}
		while (Open.Num() > 0)
		{
			const FMoveDef* M = F.Move(Open.Pop());
			for (const FChainLink& L : M->Chain)
			{
				Add(L.Move);
			}
			Add(M->ParryCounter);
		}
		return Seen;
	}

	namespace
	{
		bool IsFinisher(const FMoveDef& M) { return M.bFinisher || M.Kind == EMoveKind::Finisher || !M.StageId.IsNone(); }

		// A move of the shape's source is reached in the shape when one of its commands is not
		// shadowed by a technique the Grandmaster keeps in every shape on the same input.
		bool ReachedInShape(const FFighterDef& Shape, const FFighterDef& Source, const TSet<FName>& InShape, FName Move)
		{
			if (!InShape.Contains(Move))
			{
				return false;
			}
			TSet<FString> Kept;
			for (const FCommand& C : Shape.Commands)
			{
				if (!Source.Move(C.Move))
				{
					Kept.Add(C.Text);
				}
			}
			bool bAnyCommand = false;
			for (const TArray<FCommand>* List : {&Source.Commands, &Source.AirCommands})
			{
				for (const FCommand& C : *List)
				{
					if (C.Move == Move)
					{
						bAnyCommand = true;
						if (!Kept.Contains(C.Text))
						{
							return true;
						}
					}
				}
			}
			// Reached through a chain or a counter, not a command of its own.
			return !bAnyCommand;
		}
	}

	TArray<FCoverageRow> BuildMoveCoverage(const FContent& Content)
	{
		TArray<FCoverageRow> Rows;
		const FFighterDef* Gm = Content.Fighter(TEXT("grandmaster"));
		if (!Gm)
		{
			return Rows;
		}
		auto Sorted = [](const FFighterDef& F)
		{
			TArray<FName> Ids;
			F.Moves.GetKeys(Ids);
			Ids.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
			return Ids;
		};
		// His own techniques, in his own body and in the spent one.
		for (const FFighterDef* Own : {Gm, Gm->SpentForm})
		{
			if (!Own)
			{
				continue;
			}
			const TSet<FName> Reach = ReachableMoves(*Own);
			for (const FName& Id : Sorted(*Own))
			{
				const FMoveDef& M = *Own->Move(Id);
				FCoverageRow R;
				R.Fighter = Own->Id;
				R.Move = Id;
				R.Name = M.Name;
				if (IsFinisher(M))
				{
					R.Via = TEXT("own");
					R.Detail = TEXT("his finisher");
					R.bCovered = true;
				}
				else if (Reach.Contains(Id))
				{
					R.Via = Own == Gm ? FName(TEXT("own")) : FName(TEXT("spent"));
					R.Detail = Own->NotationOf(Id);
					R.bCovered = true;
				}
				else
				{
					R.Via = TEXT("excluded");
					R.Detail = TEXT("started by the game, not an input");
				}
				Rows.Add(R);
			}
		}
		// Every other fighter in the game, through the form library (and, for the fighters he can
		// face, the copy of the opponent).
		TArray<FName> Sources = FContent::Roster();
		Sources.Append(FContent::Special());
		for (const FName& SourceId : Sources)
		{
			const FFighterDef* Source = Content.Fighter(SourceId);
			if (!Source)
			{
				continue;
			}
			const FFighterDef* Linked = Gm->FormSources.FindRef(SourceId);
			const FName ShapeMove(*(TEXT("shape_") + SourceId.ToString()));
			const FString Input = Gm->Move(ShapeMove) ? Gm->NotationOf(ShapeMove) : FString();
			const TSharedRef<FFighterDef> Shape = MakeBorrowedShape(*Source, *Gm, false);
			const TSet<FName> Reach = ReachableMoves(*Source);
			const TSet<FName> InShape = ReachableMoves(*Shape);
			for (const FName& Id : Sorted(*Source))
			{
				const FMoveDef& M = *Source->Move(Id);
				FCoverageRow R;
				R.Fighter = SourceId;
				R.Move = Id;
				R.Name = M.Name;
				if (IsFinisher(M))
				{
					R.Via = TEXT("excluded");
					R.Detail = TEXT("a finisher: the fight's end lets the shape go");
				}
				else if (!Reach.Contains(Id))
				{
					R.Via = TEXT("excluded");
					R.Detail = TEXT("started by the game, not an input");
				}
				else
				{
					R.Via = TEXT("form");
					R.bCovered = Linked == Source && !Input.IsEmpty() && ReachedInShape(*Shape, *Source, InShape, Id);
					R.Detail = R.bCovered ? FString::Printf(TEXT("%s, then %s"), *Input, *Source->NotationOf(Id)) : TEXT("not reachable in his shape");
				}
				Rows.Add(R);
			}
		}
		// Techniques of fighters no longer in the game, carried on as his own (grandmaster.json
		// "carried"). Where the archive is present (a development checkout), every archived move
		// must be listed there.
		const TSharedPtr<FJsonObject> Carried = Json::Obj(Gm->Raw, TEXT("carried"));
		if (Carried.IsValid())
		{
			const TSet<FName> Reach = ReachableMoves(*Gm);
			for (const auto& KV : Carried->Values)
			{
				const TSharedPtr<FJsonObject> Map = KV.Value->Type == EJson::Object ? KV.Value->AsObject() : nullptr;
				if (!Map.IsValid())
				{
					continue;
				}
				TSet<FString> Listed;
				for (const auto& MV : Map->Values)
				{
					const FName To(*MV.Value->AsString());
					FCoverageRow R;
					R.Fighter = FName(*KV.Key);
					R.Move = FName(*MV.Key);
					R.Via = TEXT("carried");
					R.bCovered = Reach.Contains(To);
					R.Name = Gm->Move(To) ? Gm->Move(To)->Name : FString();
					R.Detail = FString::Printf(TEXT("%s (%s)"), *To.ToString(), R.bCovered ? *Gm->NotationOf(To) : TEXT("missing"));
					Rows.Add(R);
					Listed.Add(FString(*MV.Key));
				}
				const FString Key(*KV.Key);
				const FString Folder = Key.Left(1).ToUpper() + Key.Mid(1);
				const FString Archived = FPaths::ProjectDir() / TEXT("Archive/Slopperfall") / Folder / TEXT("Data/fighter.json");
				if (const TSharedPtr<FJsonObject> Old = FContent::LoadJson(Archived))
				{
					if (const TSharedPtr<FJsonObject> Moves = Json::Obj(Old, TEXT("moves")))
					{
						for (const auto& MV : Moves->Values)
						{
							if (!Listed.Contains(FString(*MV.Key)))
							{
								FCoverageRow R;
								R.Fighter = FName(*KV.Key);
								R.Move = FName(*MV.Key);
								R.Via = TEXT("carried");
								R.Detail = TEXT("archived but not carried over");
								Rows.Add(R);
							}
						}
					}
				}
			}
		}
		return Rows;
	}

	FString MoveCoverageReport(const TArray<FCoverageRow>& Rows)
	{
		int32 Counted = 0, Covered = 0, Excluded = 0;
		for (const FCoverageRow& R : Rows)
		{
			if (R.Via == TEXT("excluded"))
			{
				++Excluded;
				continue;
			}
			++Counted;
			Covered += R.bCovered ? 1 : 0;
		}
		FString Out = TEXT("# The Grandmaster's move coverage\n\n");
		Out += TEXT("Written by the automation test `Slop.Fighters.Grandmaster.EveryMoveIsCovered` from the loaded data ")
			TEXT("(Source/MortalSlopbat/Combat/SlopCoverage.cpp). Every move of every fighter in Ebony Hearts, and how the ")
			TEXT("Grandmaster reaches it: **own** (his own body), **spent** (his body with no Qhi), **form** (the shape from his ")
			TEXT("form library, its input, then the move's own input), **carried** (a technique of a fighter no longer in the ")
			TEXT("game, now his). Each fighter he can face can also be copied with Borrowed Face (D,B,F+Block), Qhi-enhanced. ")
			TEXT("**excluded** rows are not counted: finishers (the fight's end lets a shape go, so a shape never performs ")
			TEXT("its source's finisher) and moves the game starts itself.\n\n");
		Out += FString::Printf(TEXT("**%d of %d counted moves covered** (%d excluded).\n\n"), Covered, Counted, Excluded);
		Out += TEXT("| Fighter | Move | Name | Via | How |\n|---|---|---|---|---|\n");
		for (const FCoverageRow& R : Rows)
		{
			const TCHAR* Mark = R.Via == TEXT("excluded") ? TEXT("") : (R.bCovered ? TEXT("") : TEXT(" **NOT COVERED**"));
			Out += FString::Printf(TEXT("| %s | `%s` | %s | %s | %s%s |\n"), *R.Fighter.ToString(), *R.Move.ToString(),
				*R.Name.Replace(TEXT("|"), TEXT("/")), *R.Via.ToString(), *R.Detail.Replace(TEXT("|"), TEXT("/")), Mark);
		}
		return Out;
	}
}
