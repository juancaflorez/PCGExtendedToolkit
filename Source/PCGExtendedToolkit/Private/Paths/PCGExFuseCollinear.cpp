﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseCollinear.h"

#include "Data/PCGExDataFilter.h"

#define LOCTEXT_NAMESPACE "PCGExFuseCollinearElement"
#define PCGEX_NAMESPACE FuseCollinear

UPCGExFuseCollinearSettings::UPCGExFuseCollinearSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//PCGEX_OPERATION_DEFAULT(Blending, UPCGExSubPointsBlendInterpolate)
}

#if WITH_EDITOR
void UPCGExFuseCollinearSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExFuseCollinearSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExFuseCollinearSettings::GetPointFilterLabel() const { return PCGExDataFilter::SourceFiltersLabel; }

PCGEX_INITIALIZE_ELEMENT(FuseCollinear)

FPCGExFuseCollinearContext::~FPCGExFuseCollinearContext()
{
	PCGEX_TERMINATE_ASYNC
}


bool FPCGExFuseCollinearElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)

	//PCGEX_FWD(bDoBlend)
	//PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	return true;
}


bool FPCGExFuseCollinearElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseCollinearElement::Execute);

	PCGEX_CONTEXT(FuseCollinear)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFuseCollinear::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExFuseCollinear::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to fuse."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

namespace PCGExFuseCollinear
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseCollinear)

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		OutPoints.Add_GetRef(InPoints[0]);

		FVector LastPosition = InPoints[0].Transform.GetLocation();
		FVector CurrentDirection = (LastPosition - InPoints[1].Transform.GetLocation()).GetSafeNormal();
		const int32 MaxIndex = InPoints.Num() - 1;

		for (int i = 1; i < MaxIndex; i++)
		{
			FVector CurrentPosition = InPoints[i].Transform.GetLocation();
			FVector NextPosition = InPoints[i + 1].Transform.GetLocation();
			FVector DirToNext = (CurrentPosition - NextPosition).GetSafeNormal();

			const double Dot = FVector::DotProduct(CurrentDirection, DirToNext);
			const bool bWithinThreshold = Settings->bInvertThreshold ? Dot < Settings->Threshold : Dot > Settings->Threshold;
			if (FVector::DistSquared(CurrentPosition, LastPosition) <= Settings->FuseDistance || bWithinThreshold)
			{
				// Collinear with previous, keep moving
				continue;
			}

			OutPoints.Add_GetRef(InPoints[i]);
			CurrentDirection = DirToNext;
			LastPosition = CurrentPosition;
		}

		OutPoints.Add_GetRef(InPoints[MaxIndex]);

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
