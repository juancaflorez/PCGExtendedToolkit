﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExEdge.generated.h"

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExEdgeType : uint8
{
	Unknown  = 0 UMETA(DisplayName = "Unknown", Tooltip="Unknown type."),
	Roaming  = 1 << 0 UMETA(DisplayName = "Roaming", Tooltip="Unidirectional edge."),
	Shared   = 1 << 1 UMETA(DisplayName = "Shared", Tooltip="Shared edge, both sockets are connected; but do not match."),
	Match    = 1 << 2 UMETA(DisplayName = "Match", Tooltip="Shared relation, considered a match by the primary socket owner; but does not match on the other."),
	Complete = 1 << 3 UMETA(DisplayName = "Complete", Tooltip="Shared, matching relation on both sockets."),
	Mirror   = 1 << 4 UMETA(DisplayName = "Mirrored relation", Tooltip="Mirrored relation, connected sockets are the same on both points."),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDebugEdgeSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor ValidEdgeColor = FColor::Cyan;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin=0.1, ClampMax=10))
	double ValidEdgeThickness = 0.5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor InvalidEdgeColor = FColor::Red;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin=0.1, ClampMax=10))
	double InvalidEdgeThickness = 0.5;
};

namespace PCGExGraph
{
	const FName SourceEdgesLabel = TEXT("Edges");
	const FName OutputEdgesLabel = TEXT("Edges");

	const FName Tag_EdgeStart = TEXT("PCGEx/EdgeStart");
	const FName Tag_EdgeEnd = TEXT("PCGEx/EdgeEnd");
	const FName Tag_EdgeIndex = TEXT("PCGEx/CachedIndex");
	const FName Tag_EdgesNum = TEXT("PCGEx/CachedEdgeNum");
	const FName Tag_ClusterIndex = TEXT("PCGEx/ClusterIndex");

	const FString Tag_Cluster = FString(TEXT("PCGEx/Cluster"));

	constexpr PCGExMT::AsyncState State_ReadyForNextEdges = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingEdges = __COUNTER__;
	constexpr PCGExMT::AsyncState State_BuildingClusters = __COUNTER__;

	static uint64 GetUnsignedHash64(const uint32 A, const uint32 B)
	{
		return A > B ?
			       static_cast<uint64>(A) | (static_cast<uint64>(B) << 32) :
			       static_cast<uint64>(B) | (static_cast<uint64>(A) << 32);
	}
	
	static uint64 GetHash64(const uint32 A, const uint32 B)
	{
		return static_cast<uint64>(A) | (static_cast<uint64>(B) << 32);
	}

	static void ExpandHash64(const uint64 Hash, uint32& A, uint32& B)
	{
		A = static_cast<int32>(Hash & 0xFFFFFFFF);
		B = static_cast<int32>((Hash >> 32) & 0xFFFFFFFF);
	}
	
	struct PCGEXTENDEDTOOLKIT_API FEdge
	{
		uint32 Start = 0;
		uint32 End = 0;
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;
		bool bValid = true;

		FEdge()
		{
		}

		FEdge(const int32 InStart, const int32 InEnd):
			Start(InStart), End(InEnd)
		{
			bValid = InStart != InEnd && InStart != -1 && InEnd != -1;
		}

		FEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType)
			: FEdge(InStart, InEnd)
		{
			Type = InType;
		}

		bool Contains(const int32 InIndex) const { return Start == InIndex || End == InIndex; }

		uint32 Other(const int32 InIndex) const
		{
			check(InIndex == Start || InIndex == End)
			return InIndex == Start ? End : Start;
		}

		bool operator==(const FEdge& Other) const
		{
			return Start == Other.Start && End == Other.End;
		}

		explicit operator uint64() const
		{
			return static_cast<uint64>(Start) | (static_cast<uint64>(End) << 32);
		}

		explicit FEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>(InValue & 0xFFFFFFFF);
			End = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			Type = EPCGExEdgeType::Unknown;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FUnsignedEdge : public FEdge
	{
		FUnsignedEdge()
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd):
			FEdge(InStart, InEnd)
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			FEdge(InStart, InEnd, InType)
		{
		}

		bool operator==(const FUnsignedEdge& Other) const
		{
			return GetUnsignedHash() == Other.GetUnsignedHash();
		}

		explicit FUnsignedEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>(InValue & 0xFFFFFFFF);
			End = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			Type = EPCGExEdgeType::Unknown;
		}

		uint64 GetUnsignedHash() const { return GetUnsignedHash64(Start, End); }
	};

	struct PCGEXTENDEDTOOLKIT_API FIndexedEdge : public FUnsignedEdge
	{
		int32 EdgeIndex = -1;
		int32 PointIndex = -1;

		FIndexedEdge()
		{
		}

		FIndexedEdge(const FIndexedEdge& Other)
			: FUnsignedEdge(Other.Start, Other.End),
			  EdgeIndex(Other.EdgeIndex), PointIndex(Other.PointIndex)
		{
		}

		FIndexedEdge(const int32 InIndex, const int32 InStart, const int32 InEnd, const int32 InPointIndex = -1)
			: FUnsignedEdge(InStart, InEnd),
			  EdgeIndex(InIndex), PointIndex(InPointIndex)
		{
		}
	};

	static bool BuildIndexedEdges(
		const PCGExData::FPointIO& EdgeIO,
		const TMap<int32, int32>& NodeIndicesMap,
		TArray<FIndexedEdge>& OutEdges,
		bool bInvalidateOnError = false)
	{
		//EdgeIO.CreateInKeys();

		PCGEx::TFAttributeReader<int32>* StartIndexReader = new PCGEx::TFAttributeReader<int32>(Tag_EdgeStart);
		PCGEx::TFAttributeReader<int32>* EndIndexReader = new PCGEx::TFAttributeReader<int32>(Tag_EdgeEnd);

		if (!StartIndexReader->Bind(const_cast<PCGExData::FPointIO&>(EdgeIO))) { return false; }
		if (!EndIndexReader->Bind(const_cast<PCGExData::FPointIO&>(EdgeIO))) { return false; }

		bool bValid = true;
		const int32 NumEdges = EdgeIO.GetNum();
		int32 EdgeIndex = 0;

		OutEdges.Reserve(NumEdges);

		if (!bInvalidateOnError)
		{
			for (int i = 0; i < NumEdges; i++)
			{
				const int32* NodeStartPtr = NodeIndicesMap.Find(StartIndexReader->Values[i]);
				const int32* NodeEndPtr = NodeIndicesMap.Find(EndIndexReader->Values[i]);

				if ((!NodeStartPtr || !NodeEndPtr) ||
					(*NodeStartPtr == *NodeEndPtr)) { continue; }

				OutEdges.Emplace(EdgeIndex++, *NodeStartPtr, *NodeEndPtr, i);
			}
		}
		else
		{
			for (int i = 0; i < NumEdges; i++)
			{
				const int32* NodeStartPtr = NodeIndicesMap.Find(StartIndexReader->Values[i]);
				const int32* NodeEndPtr = NodeIndicesMap.Find(EndIndexReader->Values[i]);

				if ((!NodeStartPtr || !NodeEndPtr) ||
					(*NodeStartPtr == *NodeEndPtr))
				{
					bValid = false;
					break;
				}

				OutEdges.Emplace(EdgeIndex++, *NodeStartPtr, *NodeEndPtr, i);
			}
		}


		PCGEX_DELETE(StartIndexReader)
		PCGEX_DELETE(EndIndexReader)

		return bValid;
	}
}
