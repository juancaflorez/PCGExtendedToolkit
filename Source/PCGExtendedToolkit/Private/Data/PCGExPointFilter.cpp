﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointFilter.h"


#include "Graph/PCGExCluster.h"

TSharedPtr<PCGExPointFilter::TFilter> UPCGExFilterFactoryBase::CreateFilter() const
{
	return nullptr;
}


bool UPCGExFilterFactoryBase::Init(FPCGExContext* InContext)
{
	return true;
}

namespace PCGExPointFilter
{
	bool TFilter::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		PointDataFacade = InPointDataFacade;
		return true;
	}

	void TFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = PointDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}

	bool TFilter::Test(const int32 Index) const { return DefaultResult; }
	bool TFilter::Test(const PCGExCluster::FNode& Node) const { return Test(Node.PointIndex); }
	bool TFilter::Test(const PCGExGraph::FIndexedEdge& Edge) const { return Test(Edge.PointIndex); }

	TManager::TManager(const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
	}

	bool TManager::Init(const FPCGContext* InContext, const TArray<UPCGExFilterFactoryBase*>& InFactories)
	{
		for (const UPCGExFilterFactoryBase* Factory : InFactories)
		{
			TSharedPtr<TFilter> NewFilter = Factory->CreateFilter();
			NewFilter->bCacheResults = bCacheResultsPerFilter;
			if (!InitFilter(InContext, NewFilter)) { continue; }
			ManagedFilters.Add(NewFilter);
		}

		return PostInit(InContext);
	}

	bool TManager::Test(const int32 Index)
	{
		for (const TSharedPtr<TFilter>& Handler : ManagedFilters) { if (!Handler->Test(Index)) { return false; } }
		return true;
	}

	bool TManager::Test(const PCGExCluster::FNode& Node)
	{
		for (const TSharedPtr<TFilter>& Handler : ManagedFilters) { if (!Handler->Test(Node)) { return false; } }
		return true;
	}

	bool TManager::Test(const PCGExGraph::FIndexedEdge& Edge)
	{
		for (const TSharedPtr<TFilter>& Handler : ManagedFilters) { if (!Handler->Test(Edge)) { return false; } }
		return true;
	}

	bool TManager::InitFilter(const FPCGContext* InContext, const TSharedPtr<PCGExPointFilter::TFilter>& Filter)
	{
		return Filter->Init(InContext, PointDataFacade);
	}

	bool TManager::PostInit(const FPCGContext* InContext)
	{
		bValid = !ManagedFilters.IsEmpty();

		if (!bValid) { return false; }

		// Sort mappings so higher priorities come last, as they have to potential to override values.
		ManagedFilters.Sort([&](const TSharedPtr<TFilter>& A, const TSharedPtr<TFilter>& B) { return A->Factory->Priority < B->Factory->Priority; });

		// Update index & post-init
		for (int i = 0; i < ManagedFilters.Num(); ++i)
		{
			TSharedPtr<TFilter> Filter = ManagedFilters[i];
			Filter->FilterIndex = i;
			PostInitFilter(InContext, Filter);
		}

		if (bCacheResults) { InitCache(); }

		return true;
	}

	void TManager::PostInitFilter(const FPCGContext* InContext, const TSharedPtr<TFilter>& InFilter)
	{
		InFilter->PostInit();
	}

	void TManager::InitCache()
	{
		const int32 NumResults = PointDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}
}
