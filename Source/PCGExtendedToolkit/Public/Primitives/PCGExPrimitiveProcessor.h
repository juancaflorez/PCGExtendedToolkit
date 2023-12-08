﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPrimitiveProcessor.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPrimitiveProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPrimitiveProcessorSettings(const FObjectInitializer& ObjectInitializer);
	virtual PCGExData::EInit GetPointOutputInitMode() const override;

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PrimitiveProcessor, "PrimitiveProcessor", "Processes paths segments.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorPrimitives; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPrimitiveProcessorContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPrimitiveProcessorElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPrimitiveProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
};
