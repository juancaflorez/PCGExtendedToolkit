﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExBuildConvexHull.generated.h"

namespace PCGExGeo
{
	class TConvexHull3;
	class TDelaunayTriangulation3;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildConvexHullSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildConvexHull, "Graph : Convex Hull 3D", "Create a 3D Convex Hull triangulation for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraph; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Removes points that are not on the hull from the Vtx output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bPrunePoints = true;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle, EditCondition="!bPrunePoints"))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!bPrunePoints && bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

private:
	friend class FPCGExBuildConvexHullElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHullContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildConvexHullElement;

	virtual ~FPCGExBuildConvexHullContext() override;

	int32 ClusterUIndex = 0;

	PCGExGeo::TConvexHull3* ConvexHull = nullptr;
	TSet<int32> HullIndices;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHullElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	void WriteEdges(FPCGExBuildConvexHullContext* Context) const;
};