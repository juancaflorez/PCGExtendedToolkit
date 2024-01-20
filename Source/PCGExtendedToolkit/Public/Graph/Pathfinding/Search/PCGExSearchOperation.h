﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "UObject/Object.h"
#include "PCGExSearchOperation.generated.h"

struct FPCGExHeuristicModifiersSettings;
class UPCGExHeuristicOperation;

namespace PCGExCluster
{
	struct FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSearchOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual bool FindPath(
		const PCGExCluster::FCluster* Cluster,
		const FVector& SeedPosition,
		const FVector& GoalPosition,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers,
		TArray<int32>& OutPath);
};