﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGraph.h"
#include "PCGExEdge.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Geometry/PCGExGeoPrimtives.h"

//#include "PCGExIntersections.generated.h"


namespace PCGExGraph
{
#pragma region Compound Graph

	struct PCGEXTENDEDTOOLKIT_API FCompoundNode
	{
		const FPCGPoint Point;
		FVector Center;
		FBoxSphereBounds Bounds;
		int32 Index;

		TArray<int32> Neighbors; // PointIO Index >> Edge Index

		FCompoundNode(const FPCGPoint& InPoint, const FVector& InCenter, const int32 InIndex)
			: Point(InPoint),
			  Center(InCenter),
			  Index(InIndex)
		{
			Neighbors.Empty();
			Bounds = FBoxSphereBounds(InPoint.GetLocalBounds().TransformBy(InPoint.Transform));
		}

		~FCompoundNode()
		{
			Neighbors.Empty();
		}

		bool Add(FCompoundNode* OtherNode);

		FVector UpdateCenter(const PCGExData::FIdxCompoundList* PointsCompounds, PCGExData::FPointIOCollection* IOGroup);
	};

	struct PCGEXTENDEDTOOLKIT_API FCompoundNodeSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FCompoundNode* InNode)
		{
			return InNode->Bounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FCompoundNode* A, const FCompoundNode* B)
		{
			return A == B;
		}

		FORCEINLINE static void ApplyOffset(FCompoundNode& InNode)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FCompoundNode* Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FCompoundGraph
	{
		FVector CWTolerance;
		TMap<int64, FCompoundNode*> GridTree;

		PCGExData::FIdxCompoundList* PointsCompounds = nullptr;
		PCGExData::FIdxCompoundList* EdgesCompounds = nullptr;
		TArray<FCompoundNode*> Nodes;
		TMap<uint64, FIndexedEdge> Edges;

		const FPCGExFuseSettings FuseSettings;
		const bool bFastMode = true;

		FBox Bounds;
		const bool bFusePoints = true;

		using NodeOctree = TOctree2<FCompoundNode*, FCompoundNodeSemantics>;
		mutable NodeOctree Octree;
		mutable FRWLock OctreeLock;
		mutable FRWLock EdgesLock;

		explicit FCompoundGraph(const FPCGExFuseSettings& InFuseSettings, const FBox& InBounds, const bool FusePoints = true, const bool FastMode = true)
			: FuseSettings(InFuseSettings),
			  bFastMode(FastMode),
			  Bounds(InBounds),
			  bFusePoints(FusePoints)
		{
			Nodes.Empty();
			Edges.Empty();

			if (InFuseSettings.bComponentWiseTolerance) { FVector(1 / InFuseSettings.Tolerances.X, 1 / InFuseSettings.Tolerances.Y, 1 / InFuseSettings.Tolerances.Z); }
			else { CWTolerance = FVector(1 / InFuseSettings.Tolerance); }

			PointsCompounds = new PCGExData::FIdxCompoundList();
			EdgesCompounds = new PCGExData::FIdxCompoundList();

			if (!bFastMode) { Octree = NodeOctree(Bounds.GetCenter(), Bounds.GetExtent().Length()); }
		}

		~FCompoundGraph()
		{
			PCGEX_DELETE_TARRAY(Nodes)
			PCGEX_DELETE(PointsCompounds)
			PCGEX_DELETE(EdgesCompounds)
			Edges.Empty();
		}

		int32 NumNodes() const { return PointsCompounds->Num(); }
		int32 NumEdges() const { return EdgesCompounds->Num(); }

		FCompoundNode* GetOrCreateNode(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex);
		FCompoundNode* GetOrCreateNodeUnsafe(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex);
		PCGExData::FIdxCompound* CreateBridge(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex,
		                                      const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex,
		                                      const int32 EdgeIOIndex = -1, const int32 EdgePointIndex = -1);
		PCGExData::FIdxCompound* CreateBridgeUnsafe(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex,
		                                            const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex,
		                                            const int32 EdgeIOIndex = -1, const int32 EdgePointIndex = -1);
		void GetUniqueEdges(TArray<FUnsignedEdge>& OutEdges);
		void WriteMetadata(TMap<int32, FGraphNodeMetadata*>& OutMetadata);
	};

#pragma endregion

#pragma region Point Edge intersections

	struct PCGEXTENDEDTOOLKIT_API FPESplit
	{
		int32 NodeIndex = -1;
		double Time = -1;
		FVector ClosestPoint = FVector::ZeroVector;

		bool operator==(const FPESplit& Other) const { return NodeIndex == Other.NodeIndex; }
	};

	struct PCGEXTENDEDTOOLKIT_API FPointEdgeProxy
	{
		int32 EdgeIndex = -1;
		TArray<FPESplit> CollinearPoints;

		double LengthSquared = -1;
		double ToleranceSquared = -1;
		FBox Box = FBox(NoInit);

		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;

		FPointEdgeProxy()
		{
		}

		explicit FPointEdgeProxy(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			Init(InEdgeIndex, InStart, InEnd, Tolerance);
		}

		void Init(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			CollinearPoints.Empty();

			Start = InStart;
			End = InEnd;

			EdgeIndex = InEdgeIndex;
			ToleranceSquared = Tolerance * Tolerance;

			Box = FBox(ForceInit);
			Box += Start;
			Box += End;
			Box = Box.ExpandBy(Tolerance);

			LengthSquared = FVector::DistSquared(Start, End);
		}

		~FPointEdgeProxy()
		{
			CollinearPoints.Empty();
		}

		FORCEINLINE bool FindSplit(const FVector& Position, FPESplit& OutSplit) const;
	};

	struct PCGEXTENDEDTOOLKIT_API FPointEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		PCGExData::FPointIO* PointIO = nullptr;
		FGraph* Graph = nullptr;
		FCompoundGraph* CompoundGraph = nullptr;

		const FPCGExPointEdgeIntersectionSettings Settings;
		TArray<FPointEdgeProxy> Edges;

		FPointEdgeIntersections(
			FGraph* InGraph,
			FCompoundGraph* InCompoundGraph,
			PCGExData::FPointIO* InPointIO,
			const FPCGExPointEdgeIntersectionSettings& InSettings);

		void FindIntersections(FPCGExPointsProcessorContext* InContext);

		FORCEINLINE void Add(const int32 EdgeIndex, const FPESplit& Split);
		void Insert();

		void BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const;

		~FPointEdgeIntersections()
		{
			Edges.Empty();
		}
	};

	static void FindCollinearNodes(
		FPointEdgeIntersections* InIntersections,
		const int32 EdgeIndex,
		const UPCGPointData* PointsData)
	{
		const TArray<FPCGPoint>& Points = PointsData->GetPoints();

		const FPointEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		const FIndexedEdge& IEdge = InIntersections->Graph->Edges[EdgeIndex];
		FPESplit Split = FPESplit{};

		if (!InIntersections->Settings.bEnableSelfIntersection)
		{
			TArray<int32> IOIndices;
			InIntersections->CompoundGraph->EdgesCompounds->GetIOIndices(
				FGraphEdgeMetadata::GetRootIndex(Edge.EdgeIndex, InIntersections->Graph->EdgeMetadata), IOIndices);

			auto ProcessPointRef = [&](const FPCGPointRef& PointRef)
			{
				const ptrdiff_t PointIndex = PointRef.Point - Points.GetData();

				if (!Points.IsValidIndex(static_cast<int32>(PointIndex))) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Points[Node.PointIndex].Transform.GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (!Edge.FindSplit(Position, Split)) { return; }

				// Check overlap last as it's the most expensive op
				if (InIntersections->CompoundGraph->PointsCompounds->HasIOIndexOverlap(Node.NodeIndex, IOIndices)) { return; }

				Split.NodeIndex = Node.NodeIndex;
				InIntersections->Add(EdgeIndex, Split);
			};

			PointsData->GetOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
		}
		else
		{
			auto ProcessPointRef = [&](const FPCGPointRef& PointRef)
			{
				const ptrdiff_t PointIndex = PointRef.Point - Points.GetData();

				if (!Points.IsValidIndex(static_cast<int32>(PointIndex))) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Points[Node.PointIndex].Transform.GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (Edge.FindSplit(Position, Split))
				{
					Split.NodeIndex = Node.NodeIndex;
					InIntersections->Add(EdgeIndex, Split);
				}
			};

			PointsData->GetOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
		}
	}

#pragma endregion

#pragma region Edge Edge intersections

	struct PCGEXTENDEDTOOLKIT_API FEESplit
	{
		double TimeA = -1;
		double TimeB = -1;
		FVector Center = FVector::ZeroVector;
	};

	struct PCGEXTENDEDTOOLKIT_API FEECrossing
	{
		int32 NodeIndex = -1;
		int32 EdgeA = -1;
		int32 EdgeB = -1;
		FEESplit Split;

		explicit FEECrossing(const FEESplit& InSplit)
			: Split(InSplit)
		{
		}

		FORCEINLINE double GetTime(const int32 EdgeIndex) const { return EdgeIndex == EdgeA ? Split.TimeA : Split.TimeB; }

		bool operator==(const FEECrossing& Other) const { return NodeIndex == Other.NodeIndex; }
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeProxy
	{
		int32 EdgeIndex = -1;
		TArray<FEECrossing*> Intersections;

		double LengthSquared = -1;
		double ToleranceSquared = -1;
		FBox Box = FBox(NoInit);
		FBoxSphereBounds FSBounds = FBoxSphereBounds{};

		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;

		FEdgeEdgeProxy()
		{
		}

		explicit FEdgeEdgeProxy(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			Init(InEdgeIndex, InStart, InEnd, Tolerance);
		}

		void Init(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			Intersections.Empty();

			Start = InStart;
			End = InEnd;

			EdgeIndex = InEdgeIndex;
			ToleranceSquared = Tolerance * Tolerance;

			Box = FBox(ForceInit);
			Box += Start;
			Box += End;
			Box = Box.ExpandBy(Tolerance);

			LengthSquared = FVector::DistSquared(Start, End);
			FSBounds = Box;
		}

		~FEdgeEdgeProxy()
		{
			Intersections.Empty();
		}

		FORCEINLINE bool FindSplit(const FEdgeEdgeProxy& OtherEdge, FEESplit& OutSplit) const;
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeProxySemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FEdgeEdgeProxy* InEdge)
		{
			return InEdge->FSBounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FEdgeEdgeProxy* A, const FEdgeEdgeProxy* B)
		{
			return A == B;
		}

		FORCEINLINE static void ApplyOffset(FEdgeEdgeProxy& InEdge)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FEdgeEdgeProxy* Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		PCGExData::FPointIO* PointIO = nullptr;
		FGraph* Graph = nullptr;
		FCompoundGraph* CompoundGraph = nullptr;

		const FPCGExEdgeEdgeIntersectionSettings& Settings;

		TArray<FEECrossing*> Crossings;
		TArray<FEdgeEdgeProxy> Edges;
		TSet<uint64> CheckedPairs;

		using TEdgeOctree = TOctree2<FEdgeEdgeProxy*, FEdgeEdgeProxySemantics>;
		mutable TEdgeOctree Octree;

		FEdgeEdgeIntersections(
			FGraph* InGraph,
			FCompoundGraph* InCompoundGraph,
			PCGExData::FPointIO* InPointIO,
			const FPCGExEdgeEdgeIntersectionSettings& InSettings);

		void FindIntersections(FPCGExPointsProcessorContext* InContext);

		FORCEINLINE void Add(const int32 EdgeIndex, const int32 OtherEdgeIndex, const FEESplit& Split);
		void Insert();

		void BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const;

		~FEdgeEdgeIntersections()
		{
			CheckedPairs.Empty();
			Edges.Empty();
			PCGEX_DELETE_TARRAY(Crossings)
		}
	};

	static void FindOverlappingEdges(
		FEdgeEdgeIntersections* InIntersections,
		const int32 EdgeIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindOverlappingEdges);

		const FEdgeEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		FEESplit Split = FEESplit{};

		if (!InIntersections->Settings.bEnableSelfIntersection)
		{
			TArray<int32> IOIndices;
			InIntersections->CompoundGraph->EdgesCompounds->GetIOIndices(
				FGraphEdgeMetadata::GetRootIndex(Edge.EdgeIndex, InIntersections->Graph->EdgeMetadata), IOIndices);

			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }

				{
					FReadScopeLock ReadLock(InIntersections->InsertionLock);
					if (InIntersections->CheckedPairs.Contains(PCGEx::H64U(EdgeIndex, OtherEdge.EdgeIndex))) { return; }
				}

				if (!Edge.FindSplit(OtherEdge, Split)) { return; }

				// Check overlap last as it's the most expensive op
				if (InIntersections->CompoundGraph->EdgesCompounds->HasIOIndexOverlap(
					FGraphEdgeMetadata::GetRootIndex(OtherEdge.EdgeIndex, InIntersections->Graph->EdgeMetadata),
					IOIndices)) { return; }

				InIntersections->Add(EdgeIndex, OtherEdge.EdgeIndex, Split);
			};

			InIntersections->Octree.FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}
		else
		{
			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }

				{
					FReadScopeLock ReadLock(InIntersections->InsertionLock);
					if (InIntersections->CheckedPairs.Contains(PCGEx::H64U(EdgeIndex, OtherEdge.EdgeIndex))) { return; }
				}

				if (Edge.FindSplit(OtherEdge, Split))
				{
					InIntersections->Add(EdgeIndex, OtherEdge.EdgeIndex, Split);
				}
			};

			InIntersections->Octree.FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}
	}

#pragma endregion
}

namespace PCGExGraphTask
{
#pragma region Intersections tasks

	class PCGEXTENDEDTOOLKIT_API FFindPointEdgeIntersections final : public FPCGExNonAbandonableTask
	{
	public:
		FFindPointEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                            PCGExGraph::FPointEdgeIntersections* InIntersectionList)
			: FPCGExNonAbandonableTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FPointEdgeIntersections* IntersectionList = nullptr;
		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FInsertPointEdgeIntersections final : public FPCGExNonAbandonableTask
	{
	public:
		FInsertPointEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                              PCGExGraph::FPointEdgeIntersections* InIntersectionList)
			: FPCGExNonAbandonableTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FPointEdgeIntersections* IntersectionList = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FFindEdgeEdgeIntersections final : public FPCGExNonAbandonableTask
	{
	public:
		FFindEdgeEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                           PCGExGraph::FEdgeEdgeIntersections* InIntersectionList)
			: FPCGExNonAbandonableTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FEdgeEdgeIntersections* IntersectionList = nullptr;
		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FInsertEdgeEdgeIntersections final : public FPCGExNonAbandonableTask
	{
	public:
		FInsertEdgeEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                             PCGExGraph::FEdgeEdgeIntersections* InIntersectionList, TMap<int32, PCGExGraph::FGraphNodeMetadata*>* InOutMetadata)
			: FPCGExNonAbandonableTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FEdgeEdgeIntersections* IntersectionList = nullptr;
		TMap<int32, PCGExGraph::FGraphNodeMetadata*>* OutMetadata = nullptr;

		virtual bool ExecuteTask() override;
	};

#pragma endregion

#pragma region Compound Graph tasks

	class PCGEXTENDEDTOOLKIT_API FCompoundGraphInsertPoints final : public FPCGExNonAbandonableTask
	{
	public:
		FCompoundGraphInsertPoints(PCGExData::FPointIO* InPointIO,
		                           PCGExGraph::FCompoundGraph* InGraph)
			: FPCGExNonAbandonableTask(InPointIO),
			  Graph(InGraph)
		{
		}

		PCGExGraph::FCompoundGraph* Graph = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FCompoundGraphInsertEdges final : public FPCGExNonAbandonableTask
	{
	public:
		FCompoundGraphInsertEdges(PCGExData::FPointIO* InPointIO,
		                          PCGExGraph::FCompoundGraph* InGraph,
		                          PCGExData::FPointIO* InEdgeIO,
		                          TMap<int64, int32>* InEndpointsLookup)
			: FPCGExNonAbandonableTask(InPointIO),
			  Graph(InGraph),
			  EdgeIO(InEdgeIO),
			  EndpointsLookup(InEndpointsLookup)
		{
		}

		PCGExGraph::FCompoundGraph* Graph = nullptr;
		PCGExData::FPointIO* EdgeIO = nullptr;
		TMap<int64, int32>* EndpointsLookup = nullptr;

		virtual bool ExecuteTask() override;
	};

#pragma endregion
}
