﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPartitionVertices.h"





#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPartitionVerticesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExPartitionVerticesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

#pragma endregion

FPCGExPartitionVerticesContext::~FPCGExPartitionVerticesContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(PartitionVertices)

bool FPCGExPartitionVerticesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PartitionVertices)

	Context->VtxPartitions = MakeUnique<PCGExData::FPointIOCollection>(Context);
	Context->VtxPartitions->DefaultOutputLabel = PCGExGraph::OutputVerticesLabel;

	return true;
}

bool FPCGExPartitionVerticesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPartitionVerticesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PartitionVertices)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExPartitionVertices::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExPartitionVertices::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}

		Context->VtxPartitions->Pairs.Reserve(Context->GetClusterProcessorsNum());
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputBatches();
	Context->VtxPartitions->OutputToContext();
	Context->MainEdges->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExPartitionVertices
{
	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedPtr<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a heavy copy we'll update and forward
		return MakeShared<PCGExCluster::FCluster>(InClusterRef.Get(), VtxIO, EdgesIO, true, true, true);
	}

	FProcessor::~FProcessor()
	{
		Remapping.Empty();
		KeptIndices.Empty();
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPartitionVertices::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PartitionVertices)

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		

		Cluster->NodeIndexLookup->Empty();

		PointPartitionIO = Context->VtxPartitions->Emplace_GetRef(VtxIO, PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PointPartitionIO->GetOut()->GetMutablePoints();

		MutablePoints.SetNumUninitialized(NumNodes);
		KeptIndices.SetNumUninitialized(NumNodes);
		Remapping.Reserve(NumNodes);

		Cluster->WillModifyVtxIO();

		Cluster->VtxIO = PointPartitionIO;
		Cluster->NodeIndexLookup->Reserve(NumNodes);
		Cluster->NumRawVtx = NumNodes;

		for (PCGExCluster::FNode& Node : (*Cluster->Nodes))
		{
			int32 i = Node.NodeIndex;

			KeptIndices[i] = Node.PointIndex;
			Remapping.Add(Node.PointIndex, i);
			Cluster->NodeIndexLookup->Add(i, i); // most useless lookup in history of lookups

			Node.PointIndex = i;
		}

		StartParallelLoopForNodes();
		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		PointPartitionIO->GetOut()->GetMutablePoints()[Node.NodeIndex] = VtxIO->GetInPoint(KeptIndices[Node.NodeIndex]);
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		Edge.Start = Remapping[Edge.Start];
		Edge.End = Remapping[Edge.End];
	}

	void FProcessor::CompleteWork()
	{
		FString OutId;
		PCGExGraph::SetClusterVtx(PointPartitionIO, OutId);
		PCGExGraph::MarkClusterEdges(EdgesIO, OutId);

		ForwardCluster();
	}
}

#undef LOCTEXT_NAMESPACE
