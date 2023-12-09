﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/Tangents/PCGExAutoTangents.h"

#include "PCGExMath.h"
#include "..\..\..\Public\Data\PCGExPointIO.h"

void UPCGExAutoTangents::ProcessFirstPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& NextPoint) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex::FromStartOnly(
		NextPoint.Transform.GetLocation(),
		Point.Transform.GetLocation());

	Apex.Scale(Scale);
	WriteTangents(Point.MetadataEntry, Apex.TowardStart, Apex.TowardEnd * -1);
}

void UPCGExAutoTangents::ProcessLastPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex::FromEndOnly(
		PreviousPoint.Transform.GetLocation(),
		Point.Transform.GetLocation());

	Apex.Scale(Scale);
	WriteTangents(Point.MetadataEntry, Apex.TowardStart, Apex.TowardEnd * -1);
}

void UPCGExAutoTangents::ProcessPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex(
		PreviousPoint.Transform.GetLocation(),
		NextPoint.Transform.GetLocation(),
		Point.Transform.GetLocation());

	Apex.Scale(Scale);
	WriteTangents(Point.MetadataEntry, Apex.TowardStart, Apex.TowardEnd * -1);
}
