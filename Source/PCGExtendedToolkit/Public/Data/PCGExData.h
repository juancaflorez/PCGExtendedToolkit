﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExDataFilter.h"
#include "PCGExGlobalSettings.h"
#include "PCGExDetails.h"
#include "Data/PCGPointData.h"
#include "Geometry/PCGExGeoPointBox.h"
#include "UObject/Object.h"
#include "PCGExHelpers.h"

#include "PCGExData.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeGatherDetails : public FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExAttributeGatherDetails()
	{
		bPreservePCGExData = false;
	}

	// TODO : Expose how to handle overlaps
};

namespace PCGExData
{
	PCGEX_ASYNC_STATE(State_MergingData);

#pragma region Pool & Buffers

	static uint64 BufferUID(const FName FullName, const EPCGMetadataTypes Type)
	{
		return PCGEx::H64(GetTypeHash(FullName), static_cast<int32>(Type));
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FBufferBase
	{
		friend class FFacade;

	protected:
		mutable FRWLock BufferLock;
		mutable FRWLock WriteLock;

		bool bScopedBuffer = false;

		TArrayView<const FPCGPoint> InPoints;
		FPCGAttributeAccessorKeysPoints* InKeys = nullptr;

		TArrayView<FPCGPoint> OutPoints;
		FPCGAttributeAccessorKeysPoints* OutKeys = nullptr;

	public:
		FName FullName = NAME_None;
		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;

		const FPCGMetadataAttributeBase* InAttribute = nullptr;
		FPCGMetadataAttributeBase* OutAttribute = nullptr;

		const uint64 UID;
		FPointIO* Source = nullptr;


		FBufferBase(const FName InFullName, const EPCGMetadataTypes InType):
			FullName(InFullName), Type(InType), UID(BufferUID(FullName, Type))
		{
		}

		virtual ~FBufferBase() = default;

		virtual void Write()
		{
		}

		virtual void Fetch(const int32 StartIndex, const int32 Count)
		{
		}

		virtual bool IsScoped() { return bScopedBuffer; }
		virtual bool IsWritable() { return false; }
		virtual bool IsReadable() { return false; }

		FORCEINLINE bool GetAllowsInterpolation() const { return OutAttribute ? OutAttribute->AllowsInterpolation() : InAttribute ? InAttribute->AllowsInterpolation() : false; }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TBuffer : public FBufferBase
	{
	protected:
		TUniquePtr<FPCGAttributeAccessor<T>> InAccessor;
		const FPCGMetadataAttribute<T>* TypedInAttribute = nullptr;

		TUniquePtr<FPCGAttributeAccessor<T>> OutAccessor;
		FPCGMetadataAttribute<T>* TypedOutAttribute = nullptr;

		TArray<T>* InValues = nullptr;
		TArray<T>* OutValues = nullptr;

	public:
		T Min = T{};
		T Max = T{};

		PCGEx::TAttributeGetter<T>* ScopedBroadcaster = nullptr;

		virtual bool IsScoped() override { return bScopedBuffer || ScopedBroadcaster; }

		TBuffer(const FName InFullName, const EPCGMetadataTypes InType):
			FBufferBase(InFullName, InType)
		{
		}

		virtual ~TBuffer() override
		{
			Flush();
		}

		virtual bool IsWritable() override { return OutValues ? true : false; }
		virtual bool IsReadable() override { return InValues ? true : false; }

		TArray<T>* GetInValues() { return InValues; }
		TArray<T>* GetOutValues() { return OutValues; }
		const FPCGMetadataAttribute<T>* GetTypedInAttribute() const { return TypedInAttribute; }
		FPCGMetadataAttribute<T>* GetTypedOutAttribute() { return TypedOutAttribute; }

		FORCEINLINE T& GetMutable(const int32 Index) { return *(OutValues->GetData() + Index); }
		FORCEINLINE const T& Read(const int32 Index) const { return *(InValues->GetData() + Index); }
		FORCEINLINE const T& ReadImmediate(const int32 Index) const { return TypedInAttribute->GetValueFromItemKey(InPoints[Index]); }

		FORCEINLINE void Set(const int32 Index, const T& Value) { *(OutValues->GetData() + Index) = Value; }
		FORCEINLINE void SetImmediate(const int32 Index, const T& Value) { TypedOutAttribute->SetValue(InPoints[Index], Value); }

		void PrepareForRead(const bool bScoped, const FPCGMetadataAttributeBase* Attribute)
		{
			if (InValues) { return; }

			const TArray<FPCGPoint>& InPts = Source->GetIn()->GetPoints();
			const int32 NumPoints = InPts.Num();
			InPoints = MakeArrayView(InPts.GetData(), NumPoints);

			InKeys = Source->CreateInKeys();

			InValues = new TArray<T>();
			PCGEx::InitMetadataArray(*InValues, NumPoints);

			InAttribute = Attribute;
			TypedInAttribute = Attribute ? static_cast<const FPCGMetadataAttribute<T>*>(Attribute) : nullptr;

			bScopedBuffer = bScoped;
		}

		bool PrepareRead(const ESource InSource = ESource::In, const bool bScoped = false)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (InValues)
			{
				if (bScopedBuffer && !bScoped)
				{
					// Un-scoping reader.
					Fetch(0, InValues->Num());
					bScopedBuffer = false;
				}

				if (InSource == ESource::In && OutValues && InValues == OutValues)
				{
					// Out-source Reader was created before writer, this is bad?
					InValues = nullptr;
				}
				else
				{
					return true;
				}
			}

			if (InSource == ESource::Out)
			{
				// Reading from output
				check(OutValues)
				InValues = OutValues;
				return true;
			}

			UPCGMetadata* InMetadata = Source->GetIn()->Metadata;
			TypedInAttribute = InMetadata->GetConstTypedAttribute<T>(FullName);
			InAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedInAttribute, InMetadata);

			if (!TypedInAttribute || !InAccessor.IsValid())
			{
				TypedInAttribute = nullptr;
				InAccessor = nullptr;
				return false;
			}

			PrepareForRead(bScoped, TypedInAttribute);

			if (!bScopedBuffer)
			{
				TArrayView<T> InRange = MakeArrayView(InValues->GetData(), InValues->Num());
				InAccessor->GetRange(InRange, 0, *InKeys, EPCGAttributeAccessorFlags::StrictType);
			}

			return true;
		}

		bool PrepareWrite(T DefaultValue, bool bAllowInterpolation, const bool bUninitialized = false)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (OutValues) { return true; }

			TArray<FPCGPoint>& OutPts = Source->GetOut()->GetMutablePoints();
			const int32 NumPoints = OutPts.Num();
			OutPoints = MakeArrayView(OutPts.GetData(), NumPoints);

			OutKeys = Source->CreateOutKeys();

			UPCGMetadata* OutMetadata = Source->GetOut()->Metadata;
			TypedOutAttribute = OutMetadata->FindOrCreateAttribute(FullName, DefaultValue, bAllowInterpolation);
			OutAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedOutAttribute, OutMetadata);

			if (!TypedOutAttribute || !OutAccessor.IsValid())
			{
				TypedOutAttribute = nullptr;
				OutAccessor = nullptr;
				return false;
			}

			OutValues = new TArray<T>();
			PCGEx::InitMetadataArray(*OutValues, NumPoints);

			if (!bUninitialized)
			{
				if (Source->GetIn() && Source->GetIn()->Metadata->GetConstTypedAttribute<T>(FullName))
				{
					// TODO : Scoped get would be better here
					// Get existing values
					TArrayView<T> OutRange = MakeArrayView(OutValues->GetData(), NumPoints);
					OutAccessor->GetRange(OutRange, 0, *OutKeys, EPCGAttributeAccessorFlags::StrictType);
				}
			}

			return true;
		}

		bool PrepareWrite(const bool bUninitialized = false)
		{
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (OutValues) { return true; }
			}

			if (Source->GetIn())
			{
				if (const FPCGMetadataAttribute<T>* ExistingAttribute = Source->GetIn()->Metadata->GetConstTypedAttribute<T>(FullName))
				{
					return PrepareWrite(
						ExistingAttribute->GetValue(PCGDefaultValueKey),
						ExistingAttribute->AllowsInterpolation(),
						bUninitialized);
				}
			}

			return PrepareWrite(T{}, true, bUninitialized);
		}

		void SetScopedGetter(PCGEx::TAttributeGetter<T>* Getter)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			PrepareForRead(true, Getter->Attribute);
			ScopedBroadcaster = Getter;

			PCGEx::InitMetadataArray(*InValues, Source->GetNum());
		}

		virtual void Write() override
		{
			if (!IsWritable()) { return; }
			TArrayView<const T> View(*OutValues);
			OutAccessor->SetRange(View, 0, *OutKeys, EPCGAttributeAccessorFlags::StrictType);
		}

		virtual void Fetch(const int32 StartIndex, const int32 Count) override
		{
			if (!IsScoped()) { return; }
			if (ScopedBroadcaster) { ScopedBroadcaster->Fetch(Source, *InValues, StartIndex, Count); }
			if (InAccessor.IsValid())
			{
				TArrayView<T> ReadRange = MakeArrayView(InValues->GetData() + StartIndex, Count);
				InAccessor->GetRange(ReadRange, StartIndex, *InKeys, EPCGAttributeAccessorFlags::StrictType);
			}

			//if (OutAccessor.IsValid())
			//{
			//	TArrayView<T> WriteRange = MakeArrayView(OutValues->GetData() + StartIndex, Count);
			//	OutAccessor->GetRange(WriteRange, StartIndex, *InKeys, EPCGAttributeAccessorFlags::StrictType);
			//}
		}

		void Flush()
		{
			if (InValues) { if (InValues != OutValues) { PCGEX_DELETE(InValues) } }
			PCGEX_DELETE(OutValues)
			PCGEX_DELETE(ScopedBroadcaster)
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FFacade
	{
		mutable FRWLock PoolLock;
		mutable FRWLock CloudLock;

	public:
		FPointIO* Source = nullptr;
		TArray<FBufferBase*> Buffers;
		TMap<uint64, FBufferBase*> BufferMap;
		PCGExGeo::FPointBoxCloud* Cloud = nullptr;

		bool bSupportsScopedGet = false;

		FBufferBase* FindBufferUnsafe(const uint64 UID);
		FBufferBase* FindBuffer(const uint64 UID);

		explicit FFacade(FPointIO* InSource):
			Source(InSource)
		{
		}

		bool ShareSource(const FFacade* OtherManager) const { return this == OtherManager || OtherManager->Source == Source; }

		template <typename T>
		TBuffer<T>* FindBufferUnsafe(const FName FullName)
		{
			FBufferBase* Found = FindBufferUnsafe(BufferUID(FullName, PCGEx::GetMetadataType(T{})));
			if (!Found) { return nullptr; }
			return static_cast<TBuffer<T>*>(Found);
		}

		template <typename T>
		TBuffer<T>* FindBuffer(const FName FullName)
		{
			FBufferBase* Found = FindBuffer(BufferUID(FullName, PCGEx::GetMetadataType(T{})));
			if (!Found) { return nullptr; }
			return static_cast<TBuffer<T>*>(Found);
		}

		template <typename T>
		TBuffer<T>* GetBuffer(FName FullName)
		{
			TBuffer<T>* NewBuffer = FindBuffer<T>(FullName);
			if (NewBuffer) { return NewBuffer; }

			{
				FWriteScopeLock WriteScopeLock(PoolLock);

				NewBuffer = FindBufferUnsafe<T>(FullName);
				if (NewBuffer) { return NewBuffer; }

				NewBuffer = new TBuffer<T>(FullName, PCGEx::GetMetadataType(T{}));
				NewBuffer->Source = Source;
				Buffers.Add(NewBuffer);
				BufferMap.Add(NewBuffer->UID, NewBuffer);
				return NewBuffer;
			}
		}

		template <typename T>
		TBuffer<T>* GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false)
		{
			PCGEx::TAttributeGetter<T>* Getter;

			switch (PCGEx::GetMetadataType(T{}))
			{
			default: ;
			case EPCGMetadataTypes::Integer64:
			case EPCGMetadataTypes::Float:
			case EPCGMetadataTypes::Vector2:
			case EPCGMetadataTypes::Vector4:
			case EPCGMetadataTypes::Rotator:
			case EPCGMetadataTypes::Quaternion:
			case EPCGMetadataTypes::Transform:
			case EPCGMetadataTypes::Name:
			case EPCGMetadataTypes::Count:
			case EPCGMetadataTypes::Unknown:
				return nullptr;
			// TODO : Proper implementation, this is cursed
			case EPCGMetadataTypes::Double:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalSingleFieldGetter()));
				break;
			case EPCGMetadataTypes::Integer32:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalIntegerGetter()));
				break;
			case EPCGMetadataTypes::Vector:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalVectorGetter()));
				break;
			case EPCGMetadataTypes::Boolean:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalBoolGetter()));
				break;
			case EPCGMetadataTypes::String:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalToStringGetter()));
				break;
			}

			Getter->Capture(InSelector);
			if (!Getter->SoftGrab(Source))
			{
				PCGEX_DELETE(Getter)
				return nullptr;
			}

			TBuffer<T>* Buffer = GetBuffer<T>(Getter->FullName);

			{
				FWriteScopeLock WriteScopeLock(Buffer->BufferLock);
				Buffer->PrepareForRead(false, Getter->Attribute);
				Getter->GrabAndDump(Source, *Buffer->GetInValues(), bCaptureMinMax, Buffer->Min, Buffer->Max);
			}

			PCGEX_DELETE(Getter)

			return Buffer;
		}

		template <typename T>
		TBuffer<T>* GetBroadcaster(const FName& InName, const bool bCaptureMinMax = false)
		{
			FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
			Selector.SetAttributeName(InName);
			return GetBroadcaster<T>(Selector, bCaptureMinMax);
		}

		template <typename T>
		TBuffer<T>* GetScopedBroadcaster(const FPCGAttributePropertyInputSelector& InSelector)
		{
			if (!bSupportsScopedGet) { return GetBroadcaster<T>(InSelector); }

			PCGEx::TAttributeGetter<T>* Getter;

			switch (PCGEx::GetMetadataType(T{}))
			{
			default: ;
			case EPCGMetadataTypes::Integer64:
			case EPCGMetadataTypes::Float:
			case EPCGMetadataTypes::Vector2:
			case EPCGMetadataTypes::Vector4:
			case EPCGMetadataTypes::Rotator:
			case EPCGMetadataTypes::Quaternion:
			case EPCGMetadataTypes::Transform:
			case EPCGMetadataTypes::Name:
			case EPCGMetadataTypes::Count:
			case EPCGMetadataTypes::Unknown:
				return nullptr;
			// TODO : Proper implementation, this is cursed
			case EPCGMetadataTypes::Double:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalSingleFieldGetter()));
				break;
			case EPCGMetadataTypes::Integer32:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalIntegerGetter()));
				break;
			case EPCGMetadataTypes::Vector:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalVectorGetter()));
				break;
			case EPCGMetadataTypes::Boolean:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalBoolGetter()));
				break;
			case EPCGMetadataTypes::String:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalToStringGetter()));
				break;
			}

			Getter->Capture(InSelector);
			if (!Getter->InitForFetch(Source))
			{
				PCGEX_DELETE(Getter)
				return nullptr;
			}

			TBuffer<T>* Buffer = GetBuffer<T>(Getter->FullName);
			Buffer->SetScopedGetter(Getter);

			return Buffer;
		}

		template <typename T>
		TBuffer<T>* GetScopedBroadcaster(const FName& InName, const bool bCaptureMinMax = false)
		{
			FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
			Selector.SetAttributeName(InName);
			return GetBroadcaster<T>(Selector, bCaptureMinMax);
		}

		template <typename T>
		TBuffer<T>* GetWritable(const FPCGMetadataAttribute<T>* InAttribute, bool bUninitialized)
		{
			TBuffer<T>* Buffer = GetBuffer<T>(InAttribute->Name);
			return Buffer->PrepareWrite(InAttribute->GetValue(PCGDefaultValueKey), InAttribute->AllowsInterpolation(), bUninitialized) ? Buffer : nullptr;
		}

		template <typename T>
		TBuffer<T>* GetWritable(const FName InName, T DefaultValue, bool bAllowInterpolation, bool bUninitialized)
		{
			TBuffer<T>* Buffer = GetBuffer<T>(InName);
			return Buffer->PrepareWrite(DefaultValue, bAllowInterpolation, bUninitialized) ? Buffer : nullptr;
		}

		template <typename T>
		TBuffer<T>* GetWritable(const FName InName, bool bUninitialized)
		{
			TBuffer<T>* Buffer = GetBuffer<T>(InName);
			return Buffer->PrepareWrite(bUninitialized) ? Buffer : nullptr;
		}

		template <typename T>
		TBuffer<T>* GetReadable(const FName InName, const ESource InSource = ESource::In)
		{
			TBuffer<T>* Buffer = GetBuffer<T>(InName);
			if (!Buffer->PrepareRead(InSource, false))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
		}

		template <typename T>
		TBuffer<T>* GetScopedReadable(const FName InName)
		{
			if (!bSupportsScopedGet) { return GetReadable<T>(InName); }

			TBuffer<T>* Buffer = GetBuffer<T>(InName);
			if (!Buffer->PrepareRead(ESource::In, true))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
		}

		FPCGMetadataAttributeBase* FindMutableAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetMutableAttribute(InName);
		}

		const FPCGMetadataAttributeBase* FindConstAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetConstAttribute(InName);
		}

		template <typename T>
		FPCGMetadataAttribute<T>* FindMutableAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetMutableTypedAttribute<T>(InName);
		}

		template <typename T>
		const FPCGMetadataAttribute<T>* FindConstAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetConstTypedAttribute<T>(InName);
		}

		PCGExGeo::FPointBoxCloud* GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Epsilon = DBL_EPSILON)
		{
			FWriteScopeLock WriteScopeLock(CloudLock);

			if (Cloud) { return Cloud; }

			Cloud = new PCGExGeo::FPointBoxCloud(GetIn(), BoundsSource, Epsilon);
			return Cloud;
		}

		const UPCGPointData* GetData(const ESource InSource) const { return Source->GetData(InSource); }
		const UPCGPointData* GetIn() const { return Source->GetIn(); }
		UPCGPointData* GetOut() const { return Source->GetOut(); }

		~FFacade()
		{
			Flush();
			Source = nullptr;
			PCGEX_DELETE(Cloud)
		}

		void Flush()
		{
			PCGEX_DELETE_TARRAY(Buffers)
			BufferMap.Empty();
		}

		void Write(PCGExMT::FTaskManager* AsyncManager, const bool bFlush)
		{
			if (AsyncManager)
			{
				for (int i = 0; i < Buffers.Num(); i++)
				{
					FBufferBase* Buffer = Buffers[i];
					if (Buffer->IsWritable())
					{
						if (bFlush)
						{
							PCGExMT::WriteAndDelete(AsyncManager, Buffer);
							Buffers[i] = nullptr;
						}
						else { Buffer->Write(); }
					}
				}
			}
			else
			{
				for (FBufferBase* Buffer : Buffers) { Buffer->Write(); }
			}

			if (bFlush) { Flush(); }
		}

		void Fetch(const int32 StartIndex, const int32 Count) { for (FBufferBase* Buffer : Buffers) { Buffer->Fetch(StartIndex, Count); } }
		void Fetch(const uint64 Scope) { Fetch(PCGEx::H64A(Scope), PCGEx::H64B(Scope)); }

	protected:
		void Flush(FBufferBase* Buffer)
		{
			FWriteScopeLock WriteScopeLock(PoolLock);
			Buffers.Remove(Buffer);
			BufferMap.Remove(Buffer->UID);
			PCGEX_DELETE(Buffer)
		}
	};

	static void GetCollectionFacades(const FPointIOCollection* InCollection, TArray<FFacade*>& OutFacades)
	{
		OutFacades.Empty();
		PCGEX_SET_NUM_UNINITIALIZED(OutFacades, InCollection->Num())
		for (int i = 0; OutFacades.Num(); ++i) { OutFacades[i] = new FFacade(InCollection->Pairs[i]); }
	}

#pragma endregion

#pragma region Compound

	struct /*PCGEXTENDEDTOOLKIT_API*/ FIdxCompound
	{
	protected:
		mutable FRWLock CompoundLock;

	public:
		TSet<int32> IOIndices;
		TSet<uint64> CompoundedHashSet;

		FIdxCompound() { CompoundedHashSet.Empty(); }

		~FIdxCompound()
		{
			IOIndices.Empty();
			CompoundedHashSet.Empty();
		}

		int32 Num() const { return CompoundedHashSet.Num(); }

		void ComputeWeights(
			const TArray<FFacade*>& Sources,
			const TMap<uint32, int32>& SourcesIdx,
			const FPCGPoint& Target,
			const FPCGExDistanceDetails& InDistanceDetails,
			TArray<int32>& OutIOIdx,
			TArray<int32>& OutPointsIdx,
			TArray<double>& OutWeights) const;

		uint64 Add(const int32 IOIndex, const int32 PointIndex);

		void Clear()
		{
			IOIndices.Empty();
			CompoundedHashSet.Empty();
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FIdxCompoundList
	{
		TArray<FIdxCompound*> Compounds;

		FIdxCompoundList() { Compounds.Empty(); }
		~FIdxCompoundList() { PCGEX_DELETE_TARRAY(Compounds) }

		int32 Num() const { return Compounds.Num(); }

		FORCEINLINE FIdxCompound* New(const int32 IOIndex, const int32 PointIndex)
		{
			FIdxCompound* NewPointCompound = new FIdxCompound();
			Compounds.Add(NewPointCompound);

			NewPointCompound->IOIndices.Add(IOIndex);
			const uint64 H = PCGEx::H64(IOIndex, PointIndex);
			NewPointCompound->CompoundedHashSet.Add(H);

			return NewPointCompound;
		}

		FORCEINLINE uint64 Add(const int32 Index, const int32 IOIndex, const int32 PointIndex) { return Compounds[Index]->Add(IOIndex, PointIndex); }
		FORCEINLINE bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices)
		{
			const TSet<int32> Overlap = Compounds[InIdx]->IOIndices.Intersect(InIndices);
			return Overlap.Num() > 0;
		}

		FIdxCompound* operator[](const int32 Index) const { return this->Compounds[Index]; }
	};

#pragma endregion

#pragma region Data Marking

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(UPCGMetadata* Metadata, const FName MarkID, T MarkValue)
	{
		Metadata->DeleteAttribute(MarkID);
		FPCGMetadataAttribute<T>* Mark = Metadata->CreateAttribute<T>(MarkID, MarkValue, false, true);
		Mark->SetDefaultValue(MarkValue);
		return Mark;
	}

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(const FPointIO* PointIO, const FName MarkID, T MarkValue)
	{
		return WriteMark(PointIO->GetOut()->Metadata, MarkID, MarkValue);
	}


	template <typename T>
	static bool TryReadMark(UPCGMetadata* Metadata, const FName MarkID, T& OutMark)
	{
		const FPCGMetadataAttribute<T>* Mark = Metadata->GetConstTypedAttribute<T>(MarkID);
		if (!Mark) { return false; }
		OutMark = Mark->GetValue(PCGInvalidEntryKey);
		return true;
	}

	template <typename T>
	static bool TryReadMark(const FPointIO* PointIO, const FName MarkID, T& OutMark)
	{
		return TryReadMark(PointIO->GetIn() ? PointIO->GetIn()->Metadata : PointIO->GetOut()->Metadata, MarkID, OutMark);
	}

	static void WriteId(const FPointIO& PointIO, const FName IdName, const int64 Id)
	{
		FString OutId;
		PointIO.Tags->Add(IdName.ToString(), Id, OutId);
		if (PointIO.GetOut()) { WriteMark(PointIO.GetOut()->Metadata, IdName, Id); }
	}

	static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGPointData*>(PointData);
	}

#pragma endregion
}
