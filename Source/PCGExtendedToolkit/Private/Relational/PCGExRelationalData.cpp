﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Relational/PCGExRelationalData.h"

#include "Data/PCGPointData.h"

UPCGExRelationalData::UPCGExRelationalData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExRelationalData::IsDataReady(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	return true;
}

const TArray<FPCGExRelationalSlot>& UPCGExRelationalData::GetConstSlots()
{
	return Slots;
}

FPCGMetadataAttribute<FPCGExRelationAttributeData>* UPCGExRelationalData::PrepareData(UPCGPointData* PointData)
{
	
	int NumSlot = Slots.Num(); //Number of slots

	FPCGExRelationAttributeData Default = {};
	Default.Indices.Reserve(NumSlot);
	for (int i = 0; i < NumSlot; i++)
	{
		Default.Indices.Add(-1);
	}

	FPCGMetadataAttribute<FPCGExRelationAttributeData>* Attribute = PointData->Metadata->FindOrCreateAttribute(RelationalIdentifier, Default, false, true, true);
	return Attribute;
	
}

void UPCGExRelationalData::InitializeLocalDefinition(FPCGExRelationsDefinition Definition)
{
	Slots.Reset();
	for (FPCGExRelationalSlot Slot : Definition.Slots)
	{
		Slots.Add(Slot);
	}
}
