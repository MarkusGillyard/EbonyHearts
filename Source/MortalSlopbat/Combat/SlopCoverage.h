#pragma once

#include "CoreMinimal.h"
#include "Combat/SlopData.h"

// The Grandmaster's move coverage: every move of every fighter in the game, and how he reaches it
// (his own techniques, a shape from his form library, the copy of the opponent facing him, or a
// technique carried over from a fighter no longer in the game). Built from the loaded data, so a
// move added to any fighter shows up here on its own; Slop.Fighters.Grandmaster.EveryMoveIsCovered
// fails when one cannot be reached, and writes the report (Saved/Reports/MoveCoverage.md).
namespace Slop
{
	struct FCoverageRow
	{
		// The fighter the move belongs to ("daxion" for a carried-over move), and the move.
		FName Fighter;
		FName Move;
		FString Name;
		// How the Grandmaster reaches it: "own", "spent", "form", "carried" or "excluded".
		FName Via;
		// The input that takes the shape (for a form), the move it became (carried), or why it is
		// not counted (excluded: finishers, which a shape drops; moves the game starts itself).
		FString Detail;
		bool bCovered = false;
	};

	// Moves the fighter can start from its inputs: its ground and air commands, their chains and
	// their parry counters.
	TSet<FName> ReachableMoves(const FFighterDef& F);

	TArray<FCoverageRow> BuildMoveCoverage(const FContent& Content);
	FString MoveCoverageReport(const TArray<FCoverageRow>& Rows);
}
