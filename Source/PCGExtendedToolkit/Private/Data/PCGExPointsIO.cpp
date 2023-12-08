﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointsIO.h"

#include "PCGExMT.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
	void FPointIO::InitializeOutput(const EInit InitOut)
	{
		switch (InitOut)
		{
		case EInit::NoOutput:
			break;
		case EInit::NewOutput:
			Out = NewObject<UPCGPointData>();
			if (In) { Out->InitializeFromData(In); }
			break;
		case EInit::DuplicateInput:
			check(In)
			Out = Cast<UPCGPointData>(In->DuplicateData(true));
			break;
		case EInit::Forward:
			check(In)
			Out = const_cast<UPCGPointData*>(In);
			break;
		default: ;
		}
		if (In) { NumInPoints = In->GetPoints().Num(); }
	}

	FPointIO::FPointIO()
	{
	}

	FPointIO::FPointIO(const FName InDefaultOutputLabel, const EInit InInit): FPointIO()
	{
		DefaultOutputLabel = InDefaultOutputLabel;
		NumInPoints = 0;
		InitializeOutput(InInit);
	}

	FPointIO::FPointIO(const FPCGTaggedData& InSource, UPCGPointData* InData, const FName InDefaultOutputLabel, const EInit InInit): FPointIO()
	{
		In = const_cast<UPCGPointData*>(InData);
		DefaultOutputLabel = InDefaultOutputLabel;
		Source = InSource;
		NumInPoints = InData->GetPoints().Num();
		InitializeOutput(InInit);
	}

	const UPCGPointData* FPointIO::GetIn() const { return In; }
	int32 FPointIO::GetNum() const { return NumInPoints; }

	FPCGAttributeAccessorKeysPoints* FPointIO::GetInKeys()
	{
		if (!InKeys && In) { InKeys = new FPCGAttributeAccessorKeysPoints(In->GetPoints()); }
		return InKeys;
	}

	UPCGPointData* FPointIO::GetOut() const { return Out; }

	FPCGAttributeAccessorKeysPoints* FPointIO::GetOutKeys()
	{
		if (!OutKeys && Out)
		{
			const TArrayView<FPCGPoint> View(Out->GetMutablePoints());
			OutKeys = new FPCGAttributeAccessorKeysPoints(View);
		}
		return OutKeys;
	}


	void FPointIO::InitPoint(FPCGPoint& Point, PCGMetadataEntryKey FromKey) const
	{
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromKey, In->Metadata);
	}

	void FPointIO::InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const
	{
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromPoint.MetadataEntry, In->Metadata);
	}

	void FPointIO::InitPoint(FPCGPoint& Point) const
	{
		Out->Metadata->InitializeOnSet(Point.MetadataEntry);
	}

	FPCGPoint& FPointIO::CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		FPCGPoint& Pt = MutablePoints.Add_GetRef(FromPoint);
		OutIndex = MutablePoints.Num() - 1;
		InitPoint(Pt, FromPoint);
		return Pt;
	}

	FPCGPoint& FPointIO::NewPoint(int32& OutIndex) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		FPCGPoint& Pt = MutablePoints.Emplace_GetRef();
		OutIndex = MutablePoints.Num() - 1;
		InitPoint(Pt);
		return Pt;
	}

	void FPointIO::AddPoint(FPCGPoint& Point, int32& OutIndex, bool bInit = true) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		MutablePoints.Add(Point);
		OutIndex = MutablePoints.Num() - 1;
		if (bInit) { Out->Metadata->InitializeOnSet(Point.MetadataEntry); }
	}

	void FPointIO::AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		MutablePoints.Add(Point);
		OutIndex = MutablePoints.Num() - 1;
		InitPoint(Point, FromPoint);
	}

	UPCGPointData* FPointIO::NewEmptyOutput() const
	{
		return NewEmptyPointData(In);
	}

	UPCGPointData* FPointIO::NewEmptyOutput(FPCGContext* Context, FName PinLabel) const
	{
		UPCGPointData* OutData = NewEmptyPointData(Context, PinLabel.IsNone() ? DefaultOutputLabel : PinLabel, In);
		return OutData;
	}

	void FPointIO::Cleanup() const
	{
		if (InKeys) { delete InKeys; }
		if (OutKeys) { delete OutKeys; }
	}

	FPointIO::~FPointIO()
	{
		Cleanup();
		In = nullptr;
		Out = nullptr;
	}

	void FPointIO::BuildMetadataEntries()
	{
		if (!bMetadataEntryDirty) { return; }
		TArray<FPCGPoint>& Points = Out->GetMutablePoints();
		for (int i = 0; i < NumInPoints; i++)
		{
			FPCGPoint& Point = Points[i];
			Out->Metadata->InitializeOnSet(Point.MetadataEntry, GetInPoint(i).MetadataEntry, In->Metadata);
		}
		bMetadataEntryDirty = false;
		bIndicesDirty = true;
	}

	bool FPointIO::OutputTo(FPCGContext* Context, bool bEmplace)
	{
		if (!Out || Out->GetPoints().Num() == 0) { return false; }

		if (!bEmplace)
		{
			if (!In)
			{
				UE_LOG(LogTemp, Error, TEXT("OutputTo, bEmplace==false but no Input."));
				return false;
			}

			FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(Source);
			OutputRef.Data = Out;
			OutputRef.Pin = DefaultOutputLabel;
			Output = OutputRef;
		}
		else
		{
			FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
			OutputRef.Data = Out;
			OutputRef.Pin = DefaultOutputLabel;
			Output = OutputRef;
		}
		return true;
	}

	bool FPointIO::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
	{
		if (!Out) { return false; }

		const int64 OutNumPoints = Out->GetPoints().Num();
		if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { return false; }
		if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { return false; }

		return OutputTo(Context, bEmplace);
	}

	FPointIOGroup::FPointIOGroup()
	{
	}

	FPointIOGroup::FPointIOGroup(FPCGContext* Context, FName InputLabel, EInit InitOut)
		: FPointIOGroup()
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Context, Sources, InitOut);
	}

	FPointIOGroup::FPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, EInit InitOut)
		: FPointIOGroup()
	{
		Initialize(Context, Sources, InitOut);
	}

	FPointIOGroup::~FPointIOGroup()
	{
	}

	void FPointIOGroup::Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		const EInit InitOut)
	{
		Pairs.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGPointData* MutablePointData = GetMutablePointData(Context, Source);
			if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
			Emplace_GetRef(Source, MutablePointData, InitOut);
		}
	}

	void FPointIOGroup::Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		const EInit InitOut,
		const TFunction<bool(UPCGPointData*)>& ValidateFunc,
		const TFunction<void(FPointIO*)>& PostInitFunc)
	{
		Pairs.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			UPCGPointData* MutablePointData = GetMutablePointData(Context, Source);
			if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
			if (!ValidateFunc(MutablePointData)) { continue; }
			FPointIO* NewPointIO = Emplace_GetRef(Source, MutablePointData, InitOut);
			PostInitFunc(NewPointIO);
		}
	}

	FPointIO* FPointIOGroup::Emplace_GetRef(
		const FPointIO& PointIO,
		const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return Emplace_GetRef(PointIO.Source, PointIO.GetIn(), InitOut);
	}

	FPointIO* FPointIOGroup::Emplace_GetRef(
		const FPCGTaggedData& Source, const UPCGPointData* In,
		const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return Pairs.Emplace_GetRef(Source, In, DefaultOutputLabel, InitOut);
	}

	FPointIO* FPointIOGroup::Emplace_GetRef(
		const UPCGPointData* In,
		const EInit InitOut)
	{
		const FPCGTaggedData TaggedData;
		FWriteScopeLock WriteLock(PairsLock);
		return Pairs.Emplace_GetRef(TaggedData, In, DefaultOutputLabel, InitOut);
	}

	FPointIO* FPointIOGroup::Emplace_GetRef(const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return Pairs.Emplace_GetRef(DefaultOutputLabel, InitOut);
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context
	 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source. 
	 */
	void FPointIOGroup::OutputTo(FPCGContext* Context, const bool bEmplace)
	{
		for (FPointIO* Pair : Pairs)
		{
			Pair->OutputTo(Context, bEmplace);
		}
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context
	 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source.
	 * @param MinPointCount
	 * @param MaxPointCount 
	 */
	void FPointIOGroup::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
	{
		for (FPointIO* Pair : Pairs)
		{
			Pair->OutputTo(Context, bEmplace, MinPointCount, MaxPointCount);
		}
	}

	void FPointIOGroup::ForEach(const TFunction<void(FPointIO*, const int32)>& BodyLoop)
	{
		for (int i = 0; i < Pairs.Num(); i++)
		{
			FPointIO* PIOPair = Pairs[i];
			BodyLoop(PIOPair, i);
		}
	}

	void FPointIOGroup::Flush()
	{
		for (FPointIO* Pair : Pairs) { delete Pair; }
		Pairs.Empty();
	}
}
