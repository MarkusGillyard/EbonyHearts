#pragma once

#include "CoreMinimal.h"

namespace Slop
{
	// Frame timing for the performance record (-SlopFrameStats, any build): one CSV row per
	// rendered frame of a match with the frame, game-thread, render-thread and GPU times and the
	// process's resident memory. Tools/perf/summarize.py turns the file into the reported figures.
	class FFrameStats
	{
	public:
		explicit FFrameStats(const FString& InPath);
		~FFrameStats();
		// `Scene` names what is on screen (stage and fighters); `MatchSeconds` counts from the
		// match's first frame, so warm-up can be told apart from settled play.
		void Add(double FrameSeconds, double MatchSeconds, const FString& Scene);
		void Flush();
		const FString& GetPath() const { return Path; }

	private:
		FString Path;
		FString Pending;
		int32 Rows = 0;
		uint64 Rss = 0;
	};
}
