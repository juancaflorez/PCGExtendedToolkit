﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDeleteCustomGraph.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteCustomGraph"
#define PCGEX_NAMESPACE DeleteCustomGraph

TArray<FPCGPinProperties> UPCGExDeleteCustomGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

PCGExData::EInit UPCGExDeleteCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(DeleteCustomGraph)

bool FPCGExDeleteCustomGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteCustomGraphElement::Execute);

	PCGEX_CONTEXT(DeleteCustomGraph)

	if (!Boot(Context)) { return true; }

	while (Context->AdvancePointsIO())
	{
		auto DeleteSockets = [&](const UPCGExGraphDefinition* Params, int32)
		{
			const UPCGPointData* OutData = Context->CurrentIO->GetOut();
			for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets)
			{
				Socket.DeleteFrom(OutData);
			}
			OutData->Metadata->DeleteAttribute(Params->CachedIndexAttributeName);
		};

		Context->Graphs.ForEach(Context, DeleteSockets);
	}


	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
