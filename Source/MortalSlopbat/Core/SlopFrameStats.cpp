#include "Core/SlopFrameStats.h"
#include "DynamicRHI.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"

namespace Slop
{
	FFrameStats::FFrameStats(const FString& InPath) : Path(InPath)
	{
		FFileHelper::SaveStringToFile(TEXT("match_s,frame_ms,game_ms,render_ms,gpu_ms,rss_mb,scene\n"), *Path);
	}

	FFrameStats::~FFrameStats()
	{
		Flush();
	}

	void FFrameStats::Add(double FrameSeconds, double MatchSeconds, const FString& Scene)
	{
		// Memory is read every 30 frames; it moves slowly and the call is not free.
		if (Rows++ % 30 == 0)
		{
			Rss = FPlatformMemory::GetStats().UsedPhysical;
		}
		Pending += FString::Printf(TEXT("%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%s\n"), MatchSeconds, FrameSeconds * 1000.0,
			FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
			FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)), Rss / (1024.0 * 1024.0), *Scene);
		if (Pending.Len() > 64 * 1024)
		{
			Flush();
		}
	}

	void FFrameStats::Note(const FString& Line)
	{
		const FString Events = FPaths::ChangeExtension(Path, TEXT("events.txt"));
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s %s\n"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Line), *Events,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}

	void FFrameStats::Flush()
	{
		if (!Pending.IsEmpty())
		{
			FFileHelper::SaveStringToFile(Pending, *Path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
			Pending.Reset();
		}
	}
}
