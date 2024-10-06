﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Diagrams/PCGExBuildVoronoiGraph2D.h"

#include "PCGExRandom.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph2D

PCGExData::EInit UPCGExBuildVoronoiGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	if (bOutputSites) { PCGEX_PIN_POINTS(PCGExGraph::OutputSitesLabel, "Updated Delaunay sites.", Required, {}) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph2D)

bool FPCGExBuildVoronoiGraph2DElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	if (Settings->bOutputSites)
	{
		if (!Settings->bPruneOpenSites) { PCGEX_VALIDATE_NAME(Settings->OpenSiteFlag) }

		Context->SitesOutput = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->SitesOutput->DefaultOutputLabel = PCGExGraph::OutputSitesLabel;

		for (TSharedPtr<PCGExData::FPointIO> IO : Context->MainPoints->Pairs)
		{
			Context->SitesOutput->Emplace_GetRef(IO, PCGExData::EInit::NoOutput);
		}
	}

	return true;
}

bool FPCGExBuildVoronoiGraph2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildVoronoiGraph2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildVoronoi2D::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBuildVoronoi2D::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to build from."));
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 3 points and won't be processed."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();
	if (Context->SitesOutput) { Context->SitesOutput->StageOutputs(); }

	return Context->TryComplete();
}

namespace PCGExBuildVoronoi2D
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildVoronoi2D::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		ProjectionDetails = Settings->ProjectionDetails;
		ProjectionDetails.Init(ExecutionContext, PointDataFacade);

		// Build voronoi

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointDataFacade->GetIn()->GetPoints(), ActivePositions);

		Voronoi = MakeUnique<PCGExGeo::TVoronoi2>();

		/*
		auto ExtractValidSites = [&]()
		{
			const PCGExData::FPointIO* SitesIO = Context->SitesOutput->Pairs[BatchIndex];
			const TArray<FPCGPoint>& OriginalSites = PointIO->GetIn()->GetPoints();
			TArray<FPCGPoint>& MutableSites = SitesIO->GetOut()->GetMutablePoints();
			for (int i = 0; i < OriginalSites.Num(); i++)
			{
				if (Voronoi->Delaunay->DelaunayHull.Contains(i)) { continue; }
				MutableSites.Add(OriginalSites[i]);
			}
		};
		*/

		const FBox Bounds = PointDataFacade->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);
		bool bSuccess = false;

		if (Settings->bOutputSites)
		{
			bSuccess = Voronoi->Process<true>(ActivePositions, ProjectionDetails, Bounds, WithinBounds, VtxWithinBounds);
		}
		else
		{
			bSuccess = Voronoi->Process<false>(ActivePositions, ProjectionDetails, Bounds, WithinBounds, VtxWithinBounds);
		}

		if (!bSuccess)
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generated invalid results."));
			return false;
		}

		const int32 NumSites = Voronoi->Centroids.Num();

		ActivePositions.Empty();

		SitesPositions.SetNumUninitialized(NumSites);

		using UpdatePositionCallback = std::function<void(const int32, const FVector&)>;
		UpdatePositionCallback UpdateSitePosition = [](const int32 Site, const FVector& InCentroid)
		{
		};

		const int32 DelaunaySitesNum = PointDataFacade->GetNum(PCGExData::ESource::In);

		if (Settings->bOutputSites)
		{
			DelaunaySitesLocations.Init(FVector::ZeroVector, DelaunaySitesNum);
			DelaunaySitesInfluenceCount.Init(0, DelaunaySitesNum);

			UpdateSitePosition = [&](const int32 DelSiteIndex, const FVector& InCentroid)
			{
				DelaunaySitesLocations[DelSiteIndex] += InCentroid;
				DelaunaySitesInfluenceCount[DelSiteIndex] += 1;
			};

			SiteDataFacade = MakeShared<PCGExData::FFacade>(Context->SitesOutput->Pairs[PointDataFacade->Source->IOIndex].ToSharedRef());
			SiteDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::DuplicateInput);

			if (Settings->bPruneOutOfBounds && !Settings->bPruneOpenSites) { OpenSiteWriter = SiteDataFacade->GetWritable<bool>(Settings->OpenSiteFlag, true); }
		}

		PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(Context, PCGExData::EInit::NewOutput);

		if (Settings->Method == EPCGExCellCenter::Circumcenter && Settings->bPruneOutOfBounds)
		{
			TArray<FPCGPoint>& Centroids = PointDataFacade->GetOut()->GetMutablePoints();

			TArray<int32> RemappedIndices;
			RemappedIndices.SetNumUninitialized(NumSites);
			Centroids.Reserve(NumSites);

			for (int i = 0; i < NumSites; i++)
			{
				const FVector Centroid = Voronoi->Circumcenters[i];
				SitesPositions[i] = Centroid;

				if (!WithinBounds[i])
				{
					RemappedIndices[i] = -1;
					continue;
				}

				RemappedIndices[i] = Centroids.Num();
				FPCGPoint& NewPoint = Centroids.Emplace_GetRef();
				NewPoint.Transform.SetLocation(Centroid);
				NewPoint.Seed = PCGExRandom::ComputeSeed(NewPoint);
			}

			TArray<uint64> ValidEdges;
			ValidEdges.Reserve(Voronoi->VoronoiEdges.Num());

			if (Settings->bOutputSites)
			{
				if (Settings->bPruneOpenSites)
				{
					for (const uint64 Hash : Voronoi->VoronoiEdges)
					{
						const int32 HA = PCGEx::H64A(Hash);
						const int32 HB = PCGEx::H64B(Hash);
						const int32 A = RemappedIndices[HA];
						const int32 B = RemappedIndices[HB];

						if (A == -1 || B == -1) { continue; }
						ValidEdges.Add(PCGEx::H64(A, B));

						{
							const PCGExGeo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[HA];
							for (int i = 0; i < 3; i++) { UpdateSitePosition(Site.Vtx[i], SitesPositions[HA]); }
						}

						{
							const PCGExGeo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[HB];
							for (int i = 0; i < 3; i++) { UpdateSitePosition(Site.Vtx[i], SitesPositions[HB]); }
						}
					}
				}
				else
				{
					for (const uint64 Hash : Voronoi->VoronoiEdges)
					{
						const int32 HA = PCGEx::H64A(Hash);
						const int32 HB = PCGEx::H64B(Hash);
						const int32 A = RemappedIndices[HA];
						const int32 B = RemappedIndices[HB];

						{
							const PCGExGeo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[HA];
							for (int i = 0; i < 3; i++) { UpdateSitePosition(Site.Vtx[i], SitesPositions[HA]); }
						}

						{
							const PCGExGeo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[HB];
							for (int i = 0; i < 3; i++) { UpdateSitePosition(Site.Vtx[i], SitesPositions[HB]); }
						}

						if (A == -1 || B == -1) { continue; }
						ValidEdges.Add(PCGEx::H64(A, B));
					}
				}
			}
			else
			{
				for (const uint64 Hash : Voronoi->VoronoiEdges)
				{
					const int32 A = RemappedIndices[PCGEx::H64A(Hash)];
					const int32 B = RemappedIndices[PCGEx::H64B(Hash)];
					if (A == -1 || B == -1) { continue; }
					ValidEdges.Add(PCGEx::H64(A, B));
				}
			}

			RemappedIndices.Empty();

			GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(ValidEdges, -1);
		}
		else
		{
			TArray<FPCGPoint>& Centroids = PointDataFacade->GetOut()->GetMutablePoints();
			Centroids.SetNum(NumSites);

			if (Settings->Method == EPCGExCellCenter::Circumcenter)
			{
				for (int i = 0; i < NumSites; i++)
				{
					const FVector CC = Voronoi->Circumcenters[i];
					SitesPositions[i] = CC;
					Centroids[i].Transform.SetLocation(CC);
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Centroid)
			{
				for (int i = 0; i < NumSites; i++)
				{
					const FVector CC = Voronoi->Centroids[i];
					SitesPositions[i] = CC;
					Centroids[i].Transform.SetLocation(CC);
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Balanced)
			{
				for (int i = 0; i < NumSites; i++)
				{
					const FVector CC = WithinBounds[i] ? Voronoi->Circumcenters[i] : Voronoi->Centroids[i];
					SitesPositions[i] = CC;
					Centroids[i].Transform.SetLocation(CC);
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}


			if (Settings->bOutputSites)
			{
				for (const uint64 Hash : Voronoi->VoronoiEdges)
				{
					{
						const int32 H = PCGEx::H64A(Hash);
						const PCGExGeo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[H];
						for (int i = 0; i < 3; i++) { UpdateSitePosition(Site.Vtx[i], SitesPositions[H]); }
					}

					{
						const int32 H = PCGEx::H64B(Hash);
						const PCGExGeo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[H];
						for (int i = 0; i < 3; i++) { UpdateSitePosition(Site.Vtx[i], SitesPositions[H]); }
					}
				}
			}


			GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(Voronoi->VoronoiEdges, -1);
		}

		Voronoi.Reset();
		GraphBuilder->CompileAsync(AsyncManager, false);

		if (Settings->bOutputSites)
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, OutputSites)

			OutputSites->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const bool bIsWithinBounds = VtxWithinBounds[Index];
				if (OpenSiteWriter) { OpenSiteWriter->GetMutable(Index) = bIsWithinBounds; }
				if (DelaunaySitesInfluenceCount[Index] == 0) { return; }
				SiteDataFacade->GetOut()->GetMutablePoints()[Index].Transform.SetLocation(DelaunaySitesLocations[Index] / DelaunaySitesInfluenceCount[Index]);
			};

			OutputSites->StartIterations(DelaunaySitesNum, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		//HullMarkPointWriter->Values[Index] = Voronoi->Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput);
			return;
		}

		if (SiteDataFacade)
		{
			if (!Settings->bPruneOpenSites) { SiteDataFacade->Write(AsyncManager); }
			else
			{
				TArray<FPCGPoint>& MutablePoints = SiteDataFacade->GetOut()->GetMutablePoints();
				int32 WriteIndex = 0;
				for (int32 i = 0; i < MutablePoints.Num(); i++) { if (VtxWithinBounds[i]) { MutablePoints[WriteIndex++] = MutablePoints[i]; } }

				MutablePoints.SetNum(WriteIndex);
			}
		}

		GraphBuilder->OutputEdgesToContext();
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
