﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathsToEdgeClusters.h"

#include "Data/PCGExData.h"
#include "Graph/PCGExFindCustomGraphEdgeClusters.h"

#define LOCTEXT_NAMESPACE "PCGExPathsToEdgeClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

namespace PCGExGraph
{
}

UPCGExPathsToEdgeClustersSettings::UPCGExPathsToEdgeClustersSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExPathsToEdgeClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathsToEdgeClustersSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathsToEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPathsToEdgeClustersSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }

FName UPCGExPathsToEdgeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(PathsToEdgeClusters)

FPCGExPathsToEdgeClustersContext::~FPCGExPathsToEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(LooseNetwork)
	PCGEX_DELETE(GraphBuilder)
}

bool FPCGExPathsToEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeClusters)

	Context->LooseNetwork = new PCGExGraph::FLooseNetwork(Settings->FuseDistance);
	Context->IOIndices.Empty();

	return true;
}


bool FPCGExPathsToEdgeClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathsToEdgeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->MainPoints->ForEach(
			[&](PCGExData::FPointIO& PointIO, const int32 Index)
			{
				Context->IOIndices.Add(&PointIO, Index);
			});

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->ConsolidatedPoints = &Context->MainPoints->Emplace_GetRef();
			Context->ConsolidatedPoints->GetOut()->GetMutablePoints().SetNum(Context->LooseNetwork->Nodes.Num());

			TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();
			for (int i = 0; i < MutablePoints.Num(); i++)
			{
				MutablePoints[i].Transform.SetLocation(Context->LooseNetwork->Nodes[i]->Center);
			}

			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, 4);
			if (Settings->bFindCrossings) { Context->GraphBuilder->EnableCrossings(Settings->CrossingTolerance); }

			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
		};

		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO)
		{
			if (PointIO.GetNum() < 2) { return; }

			const int32 PrevIndex = Index - 1;
			const int32 NextIndex = Index + 1;

			PCGExGraph::FLooseNode* CurrentVtx = Context->LooseNetwork->GetLooseNode(PointIO.GetInPoint(Index));
			CurrentVtx->Add(static_cast<uint64>(*Context->IOIndices.Find(&PointIO)) | (static_cast<uint64>(Index) << 32));

			if (PointIO.GetIn()->GetPoints().IsValidIndex(PrevIndex))
			{
				PCGExGraph::FLooseNode* OtherVtx = Context->LooseNetwork->GetLooseNode(PointIO.GetInPoint(PrevIndex));
				CurrentVtx->Add(OtherVtx);
			}

			if (PointIO.GetIn()->GetPoints().IsValidIndex(NextIndex))
			{
				PCGExGraph::FLooseNode* OtherVtx = Context->LooseNetwork->GetLooseNode(PointIO.GetInPoint(NextIndex));
				CurrentVtx->Add(OtherVtx);
			}
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint, true)) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		// Build Network
		auto InsertEdge = [&](const int32 NodeIndex)
		{
			PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};
			const PCGExGraph::FLooseNode* Node = Context->LooseNetwork->Nodes[NodeIndex];
			for (const int32 OtherNodeIndex : Node->Neighbors)
			{
				Context->GraphBuilder->Graph->InsertEdge(Node->Index, OtherNodeIndex, NewEdge);
			}
		};

		if (!Context->Process(InsertEdge, Context->LooseNetwork->Nodes.Num())) { return false; }
		if (Context->GraphBuilder->EdgeCrossings) { Context->SetState(PCGExGraph::State_FindingCrossings); }
		else { Context->SetState(PCGExGraph::State_WritingClusters); }
	}

	if (Context->IsState(PCGExGraph::State_FindingCrossings))
	{
		auto Initialize = [&]()
		{
			Context->GraphBuilder->EdgeCrossings->Prepare(Context->ConsolidatedPoints->GetOut()->GetPoints());
		};

		auto ProcessEdge = [&](const int32 Index)
		{
			Context->GraphBuilder->EdgeCrossings->ProcessEdge(Index, Context->ConsolidatedPoints->GetOut()->GetPoints());
		};

		if (!Context->Process(Initialize, ProcessEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }
		Context->SetState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			Context->GraphBuilder->Write(Context);
			Context->OutputPoints();
		}
		Context->Done();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE