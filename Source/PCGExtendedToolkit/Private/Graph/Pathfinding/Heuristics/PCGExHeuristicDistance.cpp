﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

void UPCGExHeuristicDistance::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	InCluster->ComputeEdgeLengths(true);
	Super::PrepareForData(InCluster);
}

double UPCGExHeuristicDistance::ComputeScore(
	const PCGExCluster::FScoredNode* From,
	const PCGExCluster::FNode& To,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal, const PCGExGraph::FIndexedEdge& Edge) const
{
	return From->Score + Cluster->EdgeLengths[Edge.EdgeIndex] * ReferenceWeight;
}
