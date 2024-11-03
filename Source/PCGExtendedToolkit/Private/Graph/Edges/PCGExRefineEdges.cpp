﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRefineEdges.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExRefineEdges"
#define PCGEX_NAMESPACE RefineEdges

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Refinement && Refinement->RequiresHeuristics()) { PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics may be required by some refinements.", Required, {}) }
	if (Refinement && Refinement->SupportFilters())
	{
		//PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceVtxFilters, "Filters used to check if a vtx should be processed.", Normal, {})
		PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceEdgeFilters, "Refinements filters.", Normal, {})
	}

	if (Sanitization == EPCGExRefineSanitization::Filters)
	{
		PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceSanitizeEdgeFilters, "Filters that define which edges are to be kept. During the sanitization step, edges that pass the filters are restored if they were previously removed.", Required, {})
	}

	PCGEX_PIN_OPERATION_OVERRIDES(PCGExRefineEdges::SourceOverridesRefinement)

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::OutputPinProperties() const
{
	if (!bOutputEdgesOnly) { return Super::OutputPinProperties(); }

	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputKeptEdgesLabel, "Kept edges but as simple points.", Required, {})
	PCGEX_PIN_POINTS(PCGExGraph::OutputRemovedEdgesLabel, "Removed edges but as simple points.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExRefineEdgesSettings::GetMainOutputInitMode() const { return bOutputEdgesOnly ? PCGExData::EInit::NoOutput : PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExRefineEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(RefineEdges)

bool FPCGExRefineEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	if (!Settings->Refinement)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No refinement selected."));
		return false;
	}

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefineOperation, PCGExRefineEdges::SourceOverridesRefinement)
	PCGEX_FWD(GraphBuilderDetails)

	if (Context->Refinement->RequiresHeuristics() && !Context->bHasValidHeuristics)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("The selected refinement requires heuristics to be connected, but none can be found."));
		return false;
	}

	if (Context->Refinement->SupportFilters())
	{
		//GetInputFactories(Context, PCGExRefineEdges::SourceVtxFilters, Context->VtxFilterFactories, PCGExFactories::ClusterNodeFilters, false);
		GetInputFactories(Context, PCGExRefineEdges::SourceEdgeFilters, Context->EdgeFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	}

	if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
	{
		if (!GetInputFactories(Context, PCGExRefineEdges::SourceSanitizeEdgeFilters, Context->SanitizationFilterFactories, PCGExFactories::ClusterEdgeFilters, true))
		{
			return false;
		}
	}

	if (Settings->bOutputEdgesOnly)
	{
		Context->KeptEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->KeptEdges->DefaultOutputLabel = PCGExGraph::OutputKeptEdgesLabel;

		Context->RemovedEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->RemovedEdges->DefaultOutputLabel = PCGExGraph::OutputRemovedEdgesLabel;

		int32 NumEdgesInputs = Context->MainEdges->Num();
		Context->KeptEdges->Pairs.Reserve(NumEdgesInputs);
		Context->RemovedEdges->Pairs.Reserve(NumEdgesInputs);

		for (const TSharedPtr<PCGExData::FPointIO>& EdgeIO : Context->MainEdges->Pairs)
		{
			Context->KeptEdges->Emplace_GetRef(EdgeIO, PCGExData::EInit::NewOutput)->bAllowEmptyOutput = Settings->bAllowZeroPointOutputs;
			Context->RemovedEdges->Emplace_GetRef(EdgeIO, PCGExData::EInit::NewOutput)->bAllowEmptyOutput = Settings->bAllowZeroPointOutputs;
		}
	}

	return true;
}

bool FPCGExRefineEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRefineEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (Settings->bOutputEdgesOnly)
		{
			if (!Context->StartProcessingClusters<PCGExRefineEdges::FProcessorBatch>(
				[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
				[&](const TSharedPtr<PCGExRefineEdges::FProcessorBatch>& NewBatch)
				{
					if (Context->Refinement->RequiresHeuristics()) { NewBatch->SetRequiresHeuristics(true); }
				}))
			{
				return Context->CancelExecution(TEXT("Could not build any clusters."));
			}
		}
		else
		{
			if (!Context->StartProcessingClusters<PCGExRefineEdges::FProcessorBatch>(
				[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
				[&](const TSharedPtr<PCGExRefineEdges::FProcessorBatch>& NewBatch)
				{
					NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
					if (Context->Refinement->RequiresHeuristics()) { NewBatch->SetRequiresHeuristics(true); }
				}))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));

				return true;
			}
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(Settings->bOutputEdgesOnly ? PCGEx::State_Done : PCGExGraph::State_ReadyToCompile)

	if (!Settings->bOutputEdgesOnly && !Context->CompileGraphBuilders(true, PCGEx::State_Done)) { return false; }

	if (!Settings->bOutputEdgesOnly)
	{
		Context->MainPoints->StageOutputs();
	}
	else
	{
		Context->KeptEdges->StageOutputs();
		Context->RemovedEdges->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExRefineEdges
{
	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a light working copy with edges only, will be deleted.
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
			false, true, false);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExRefineEdges::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		Sanitization = Settings->Sanitization;

		Refinement = Context->Refinement->CopyOperation<UPCGExEdgeRefineOperation>();
		Refinement->PrimaryDataFacade = VtxDataFacade;
		Refinement->SecondaryDataFacade = EdgeDataFacade;

		Refinement->PrepareForCluster(Cluster, HeuristicsHandler);

		Refinement->EdgesFilters = &EdgeFilterCache;
		EdgeFilterCache.Init(true, EdgeDataFacade->Source->GetNum());

		if (!Context->EdgeFilterFactories.IsEmpty())
		{
			EdgeFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
			EdgeFilterManager->bUseEdgeAsPrimary = true;
			if (!EdgeFilterManager->Init(ExecutionContext, Context->EdgeFilterFactories)) { return false; }
		}
		else
		{
		}

		const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize();

		if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
		{
			if (!Context->SanitizationFilterFactories.IsEmpty())
			{
				SanitizationFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
				SanitizationFilterManager->bUseEdgeAsPrimary = true;
				if (!SanitizationFilterManager->Init(ExecutionContext, Context->SanitizationFilterFactories)) { return false; }
			}
		}

		// Need to go through PrepareSingleLoopScopeForEdges anyway

		if (Refinement->RequiresIndividualEdgeProcessing())
		{
			StartParallelLoopForEdges();
		}
		else
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, EdgeScopeLoop)
			EdgeScopeLoop->OnCompleteCallback =
				[&]()
				{
					if (Refinement->RequiresIndividualNodeProcessing()) { StartParallelLoopForNodes(); }
					else { Refinement->Process(); }
				};

			EdgeScopeLoop->OnSubLoopStartCallback =
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { PrepareSingleLoopScopeForEdges(StartIndex, Count); };

			EdgeScopeLoop->StartSubLoops(EdgeDataFacade->GetNum(), PLI);
		}

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		Refinement->ProcessNode(Node);
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count)
	{
		const int32 MaxIndex = StartIndex + Count;

		EdgeDataFacade->Fetch(StartIndex, Count);

		TArray<PCGExGraph::FIndexedEdge>& Edges = *Cluster->Edges;

		const bool bDefaultValidity = Refinement->GetDefaultEdgeValidity();

		if (EdgeFilterManager)
		{
			for (int i = StartIndex; i < MaxIndex; i++)
			{
				EdgeFilterCache[i] = EdgeFilterManager->Test(Edges[i]);
				Edges[i].bValid = bDefaultValidity;
			}
		}
		else
		{
			for (int i = StartIndex; i < MaxIndex; i++)
			{
				Edges[i].bValid = bDefaultValidity;
			}
		}
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		Refinement->ProcessEdge(Edge);
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		if (!Settings->bRestoreEdgesThatConnectToValidNodes) { return; }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, InvalidateNodes)

		InvalidateNodes->OnSubLoopStartCallback =
			[WeakThis = TWeakPtr<FProcessor>(SharedThis(this))]
			(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const TSharedPtr<FProcessor> This = WeakThis.Pin();
				if (!This) { return; }

				const PCGExCluster::FCluster* Cluster = This->Cluster.Get();
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++)
				{
					if (PCGExCluster::FNode* Node = Cluster->GetNode(i); !Node->HasAnyValidEdges(Cluster)) { Node->bValid = false; }
				}
			};

		InvalidateNodes->OnCompleteCallback =
			[WeakThis = TWeakPtr<FProcessor>(SharedThis(this))]()
			{
				const TSharedPtr<FProcessor> This = WeakThis.Pin();
				if (!This) { return; }

				PCGEX_ASYNC_GROUP_CHKD_VOID(This->AsyncManager, RestoreEdges)
				RestoreEdges->OnSubLoopStartCallback = [WeakThis](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
				{
					const TSharedPtr<FProcessor> NestedThis = WeakThis.Pin();
					if (!NestedThis) { return; }

					const PCGExCluster::FCluster* Cluster = NestedThis->Cluster.Get();

					const int32 MaxIndex = StartIndex + Count;
					for (int i = StartIndex; i < MaxIndex; i++)
					{
						PCGExGraph::FIndexedEdge* Edge = NestedThis->Cluster->GetEdge(i);
						if (Edge->bValid) { continue; }
						if (Cluster->GetEdgeStart(Edge->EdgeIndex)->bValid && Cluster->GetEdgeStart(Edge->EdgeIndex)->bValid) { Edge->bValid = true; }
					}
				};

				RestoreEdges->StartSubLoops(This->Cluster->Edges->Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
			};


		InvalidateNodes->StartSubLoops(Cluster->Nodes->Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::Sanitize()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, SanitizeTaskGroup)

		Cluster->GetExpandedEdges(true); //Oof

		SanitizeTaskGroup->OnCompleteCallback = [&]() { InsertEdges(); };

		if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
		{
			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize();
			SanitizeTaskGroup->OnSubLoopStartCallback =
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
				{
					const int32 MaxIndex = StartIndex + Count;
					for (int i = StartIndex; i < MaxIndex; i++)
					{
						PCGExGraph::FIndexedEdge& Edge = *Cluster->GetEdge(i);
						if (SanitizationFilterManager->Test(Edge)) { Edge.bValid = true; }
					}
				};
			SanitizeTaskGroup->StartSubLoops(EdgeDataFacade->GetNum(), PLI);
		}
		else
		{
			SanitizeTaskGroup->StartRanges<FSanitizeRangeTask>(
				NumNodes, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(),
				nullptr, SharedThis(this));
		}
	}

	void FProcessor::InsertEdges() const
	{
		if (GraphBuilder)
		{
			TArray<PCGExGraph::FIndexedEdge> ValidEdges;
			Cluster->GetValidEdges(ValidEdges);

			if (ValidEdges.IsEmpty()) { return; }

			GraphBuilder->Graph->InsertEdges(ValidEdges);
			return;
		}

		const TArray<FPCGPoint>& OriginalEdges = EdgeDataFacade->GetIn()->GetPoints();
		const int32 EdgesNum = OriginalEdges.Num();

		TArray<FPCGPoint>& KeptEdges = Context->KeptEdges->Pairs[EdgeDataFacade->Source->IOIndex]->GetMutablePoints();
		TArray<FPCGPoint>& RemovedEdges = Context->RemovedEdges->Pairs[EdgeDataFacade->Source->IOIndex]->GetMutablePoints();

		KeptEdges.Reserve(EdgesNum);
		RemovedEdges.Reserve(EdgesNum);

		const TArray<PCGExGraph::FIndexedEdge>& Edges = *Cluster->Edges;
		for (int i = 0; i < EdgesNum; i++)
		{
			if (Edges[i].bValid) { KeptEdges.Add(OriginalEdges[i]); }
			else { RemovedEdges.Add(OriginalEdges[i]); }
		}

		KeptEdges.SetNum(KeptEdges.Num());
		RemovedEdges.SetNum(RemovedEdges.Num());
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->Sanitization != EPCGExRefineSanitization::None)
		{
			Sanitize();
			return;
		}

		InsertEdges();
	}

	void FProcessorBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		Context->Refinement->RegisterBuffersDependencies(ExecutionContext, FacadePreloader);

		//PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->VtxFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->EdgeFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->SanitizationFilterFactories, FacadePreloader);
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		Context->Refinement->PrepareVtxFacade(VtxDataFacade);
		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FSanitizeRangeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		const int32 StartIndex = PCGEx::H64A(Scope);
		const int32 NumIterations = PCGEx::H64B(Scope);

		auto RestoreEdge = [&](const int32 EdgeIndex)
		{
			if (EdgeIndex == -1) { return; }
			FPlatformAtomics::InterlockedExchange(&Processor->Cluster->GetEdge(EdgeIndex)->bValid, 1);
			FPlatformAtomics::InterlockedExchange(&Processor->Cluster->GetEdgeStart(EdgeIndex)->bValid, 1);
			FPlatformAtomics::InterlockedExchange(&Processor->Cluster->GetEdgeEnd(EdgeIndex)->bValid, 1);
		};

		if (Processor->Sanitization == EPCGExRefineSanitization::Longest)
		{
			for (int i = 0; i < NumIterations; i++)
			{
				const PCGExCluster::FNode* Node = (Processor->Cluster->GetNode(StartIndex + i));

				int32 BestIndex = -1;
				double LongestDist = MIN_dbl;

				for (const uint64 AdjacencyHash : Node->Adjacency)
				{
					uint32 OtherNodeIndex;
					uint32 EdgeIndex;
					PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

					const double Dist = Processor->Cluster->GetDistSquared(Node->NodeIndex, OtherNodeIndex);
					if (Dist > LongestDist)
					{
						LongestDist = Dist;
						BestIndex = EdgeIndex;
					}
				}

				RestoreEdge(BestIndex);
			}
		}
		else if (Processor->Sanitization == EPCGExRefineSanitization::Shortest)
		{
			for (int i = 0; i < NumIterations; i++)
			{
				const PCGExCluster::FNode* Node = Processor->Cluster->GetNode(StartIndex + i);

				int32 BestIndex = -1;
				double ShortestDist = MAX_dbl;

				for (const uint64 AdjacencyHash : Node->Adjacency)
				{
					uint32 OtherNodeIndex;
					uint32 EdgeIndex;
					PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

					const double Dist = Processor->Cluster->GetDistSquared(Node->NodeIndex, OtherNodeIndex);
					if (Dist < ShortestDist)
					{
						ShortestDist = Dist;
						BestIndex = EdgeIndex;
					}
				}

				RestoreEdge(BestIndex);
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
