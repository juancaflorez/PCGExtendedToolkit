﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExHelpers.h"
#include "PCGExMath.h"
#include "PCGExMT.h"
#include "PCGExPointIO.h"
#include "Blending/PCGExBlendModes.h"

#include "Metadata/Accessors/PCGAttributeAccessor.h"

#include "PCGExAttributeHelpers.generated.h"

#pragma region Input Configs

namespace PCGExData
{
	class FFacade;
}

struct FPCGExAttributeGatherDetails;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputConfig
{
	GENERATED_BODY()

	FPCGExInputConfig() = default;
	explicit FPCGExInputConfig(const FPCGAttributePropertyInputSelector& InSelector);
	explicit FPCGExInputConfig(const FPCGExInputConfig& Other);
	explicit FPCGExInputConfig(const FName InName);

	virtual ~FPCGExInputConfig() = default;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (HideInDetailPanel, Hidden, EditConditionHides, EditCondition="false"))
	FString TitlePropertyName;

	/** Attribute or $Property. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	FPCGAttributePropertyInputSelector Selector;

	FPCGMetadataAttributeBase* Attribute = nullptr;
	int16 UnderlyingType = static_cast<int16>(EPCGMetadataTypes::Unknown);

	FPCGAttributePropertyInputSelector& GetMutableSelector() { return Selector; }

	EPCGAttributePropertySelection GetSelection() const { return Selector.GetSelection(); }
	FName GetName() const { return Selector.GetName(); }
#if WITH_EDITOR
	virtual FString GetDisplayName() const;
	void UpdateUserFacingInfos();
#endif
	/**
	 * Bind & cache the current selector for a given UPCGPointData
	 * @param InData 
	 * @return 
	 */
	virtual bool Validate(const UPCGPointData* InData);
	FString ToString() const { return GetName().ToString(); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeSourceToTargetDetails
{
	GENERATED_BODY()

	FPCGExAttributeSourceToTargetDetails()
	{
	}

	/** Attribute to read on input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Source = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOutputToDifferentName = false;

	/** Attribute to write on output, if different from input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputToDifferentName"))
	FName Target = NAME_None;

	bool ValidateNames(FPCGExContext* InContext) const;
	FName GetOutputName() const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeSourceToTargetList
{
	GENERATED_BODY()

	FPCGExAttributeSourceToTargetList()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{Source}"))
	TArray<FPCGExAttributeSourceToTargetDetails> Attributes;

	bool IsEmpty() const { return Attributes.IsEmpty(); }
	int32 Num() const { return Attributes.Num(); }

	bool ValidateNames(FPCGExContext* InContext) const;
	void SetOutputTargetNames(const TSharedRef<PCGExData::FFacade>& InFacade) const;

	void GetSources(TArray<FName>& OutNames) const;
};

#pragma endregion

namespace PCGEx
{
#pragma region Field helpers

	static const TMap<FString, EPCGExTransformComponent> STRMAP_TRANSFORM_FIELD = {
		{TEXT("POSITION"), EPCGExTransformComponent::Position},
		{TEXT("POS"), EPCGExTransformComponent::Position},
		{TEXT("ROTATION"), EPCGExTransformComponent::Rotation},
		{TEXT("ROT"), EPCGExTransformComponent::Rotation},
		{TEXT("ORIENT"), EPCGExTransformComponent::Rotation},
		{TEXT("SCALE"), EPCGExTransformComponent::Scale},
	};

	static const TMap<FString, EPCGExSingleField> STRMAP_SINGLE_FIELD = {
		{TEXT("X"), EPCGExSingleField::X},
		{TEXT("R"), EPCGExSingleField::X},
		{TEXT("ROLL"), EPCGExSingleField::X},
		{TEXT("RX"), EPCGExSingleField::X},
		{TEXT("Y"), EPCGExSingleField::Y},
		{TEXT("G"), EPCGExSingleField::Y},
		{TEXT("YAW"), EPCGExSingleField::Y},
		{TEXT("RY"), EPCGExSingleField::Y},
		{TEXT("Z"), EPCGExSingleField::Z},
		{TEXT("B"), EPCGExSingleField::Z},
		{TEXT("P"), EPCGExSingleField::Z},
		{TEXT("PITCH"), EPCGExSingleField::Z},
		{TEXT("RZ"), EPCGExSingleField::Z},
		{TEXT("W"), EPCGExSingleField::W},
		{TEXT("A"), EPCGExSingleField::W},
		{TEXT("L"), EPCGExSingleField::Length},
		{TEXT("LEN"), EPCGExSingleField::Length},
		{TEXT("LENGTH"), EPCGExSingleField::Length},
		{TEXT("SQUAREDLENGTH"), EPCGExSingleField::SquaredLength},
		{TEXT("LENSQR"), EPCGExSingleField::SquaredLength},
		{TEXT("VOL"), EPCGExSingleField::Volume},
		{TEXT("VOLUME"), EPCGExSingleField::Volume}
	};

	static const TMap<FString, EPCGExAxis> STRMAP_AXIS = {
		{TEXT("FORWARD"), EPCGExAxis::Forward},
		{TEXT("FRONT"), EPCGExAxis::Forward},
		{TEXT("BACKWARD"), EPCGExAxis::Backward},
		{TEXT("BACK"), EPCGExAxis::Backward},
		{TEXT("RIGHT"), EPCGExAxis::Right},
		{TEXT("LEFT"), EPCGExAxis::Left},
		{TEXT("UP"), EPCGExAxis::Up},
		{TEXT("TOP"), EPCGExAxis::Up},
		{TEXT("DOWN"), EPCGExAxis::Down},
		{TEXT("BOTTOM"), EPCGExAxis::Down},
	};

	bool GetComponentSelection(const TArray<FString>& Names, EPCGExTransformComponent& OutSelection);
	bool GetFieldSelection(const TArray<FString>& Names, EPCGExSingleField& OutSelection);
	bool GetAxisSelection(const TArray<FString>& Names, EPCGExAxis& OutSelection);

#pragma endregion

	FPCGAttributePropertyInputSelector CopyAndFixLast(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, TArray<FString>& OutExtraNames);

#pragma region Attribute identity

	struct PCGEXTENDEDTOOLKIT_API FAttributeIdentity
	{
		FName Name = NAME_None;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		bool bAllowsInterpolation = true;

		FAttributeIdentity() = default;

		FAttributeIdentity(const FAttributeIdentity& Other)
			: Name(Other.Name), UnderlyingType(Other.UnderlyingType), bAllowsInterpolation(Other.bAllowsInterpolation)
		{
		}

		FAttributeIdentity(const FName InName, const EPCGMetadataTypes InUnderlyingType, const bool InAllowsInterpolation)
			: Name(InName), UnderlyingType(InUnderlyingType), bAllowsInterpolation(InAllowsInterpolation)
		{
		}

		int16 GetTypeId() const { return static_cast<int16>(UnderlyingType); }
		bool IsA(const int16 InType) const { return GetTypeId() == InType; }
		bool IsA(const EPCGMetadataTypes InType) const { return UnderlyingType == InType; }

		FString GetDisplayName() const { return FString(Name.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType)); }
		bool operator==(const FAttributeIdentity& Other) const { return Name == Other.Name; }

		static void Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities);
		static void Get(const UPCGMetadata* InMetadata, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities);

		using FForEachFunc = std::function<void (const FAttributeIdentity&, const int32)>;
		static int32 ForEach(const UPCGMetadata* InMetadata, FForEachFunc&& Func);
	};

	class FAttributesInfos : public TSharedFromThis<FAttributesInfos>
	{
	public:
		TMap<FName, int32> Map;
		TArray<FAttributeIdentity> Identities;
		TArray<FPCGMetadataAttributeBase*> Attributes;

		bool Contains(FName AttributeName, EPCGMetadataTypes Type);
		bool Contains(FName AttributeName);
		FAttributeIdentity* Find(FName AttributeName);

		bool FindMissing(const TSet<FName>& Checklist, TSet<FName>& OutMissing);
		bool FindMissing(const TArray<FName>& Checklist, TSet<FName>& OutMissing);

		void Append(const TSharedPtr<FAttributesInfos>& Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);
		void Append(const TSharedPtr<FAttributesInfos>& Other, TSet<FName>& OutTypeMismatch, const TSet<FName>* InIgnoredAttributes = nullptr);
		void Update(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);

		using FilterCallback = std::function<bool(const FName&)>;

		void Filter(const FilterCallback& FilterFn);

		~FAttributesInfos() = default;

		static TSharedPtr<FAttributesInfos> Get(const UPCGMetadata* InMetadata, const TSet<FName>* IgnoredAttributes = nullptr);
		static TSharedPtr<FAttributesInfos> Get(const TSharedPtr<PCGExData::FPointIOCollection>& InCollection, TSet<FName>& OutTypeMismatch, const TSet<FName>* IgnoredAttributes = nullptr);
	};

	static void GatherAttributes(
		const TSharedPtr<FAttributesInfos>& OutInfos, const FPCGContext* InContext, const FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		TArray<FPCGTaggedData> InputData = InContext->InputData.GetInputsByPin(InputLabel);
		for (const FPCGTaggedData& TaggedData : InputData)
		{
			if (const UPCGParamData* AsParamData = Cast<UPCGParamData>(TaggedData.Data))
			{
				OutInfos->Append(FAttributesInfos::Get(AsParamData->Metadata), InDetails, Mismatches);
			}
			else if (const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(TaggedData.Data))
			{
				OutInfos->Append(FAttributesInfos::Get(AsSpatialData->Metadata), InDetails, Mismatches);
			}
		}
	}

	static TSharedPtr<FAttributesInfos> GatherAttributes(
		const FPCGContext* InContext, const FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		PCGEX_MAKE_SHARED(OutInfos, FAttributesInfos)
		GatherAttributes(OutInfos, InContext, InputLabel, InDetails, Mismatches);
		return OutInfos;
	}

#pragma endregion

#pragma region Local Attribute Inputs

	template <bool bInitialized>
	static FName GetSelectorFullName(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
	{
		if (!InData) { return FName(TEXT("NULL_DATA")); }

		if constexpr (bInitialized)
		{
			if (InSelector.GetExtraNames().IsEmpty()) { return FName(InSelector.GetName().ToString()); }
			return FName(InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT(".")));
		}
		else
		{
			if (InSelector.GetSelection() == EPCGAttributePropertySelection::Attribute && InSelector.GetName() == "@Last")
			{
				return GetSelectorFullName<true>(InSelector.CopyAndFixLast(InData), InData);
			}

			return GetSelectorFullName<true>(InSelector, InData);
		}
	}

	static FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector)
	{
		if (InSelector.GetExtraNames().IsEmpty()) { return InSelector.GetName().ToString(); }
		return InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT("."));
	}

	class PCGEXTENDEDTOOLKIT_API FAttributeBroadcasterBase
	{
	public:
		virtual ~FAttributeBroadcasterBase() = default;
	};

	template <typename T>
	class TAttributeBroadcaster : public FAttributeBroadcasterBase
	{
	protected:
		TSharedPtr<PCGExData::FPointIO> PointIO;
		bool bMinMaxDirty = true;
		bool bNormalized = false;
		FPCGAttributePropertyInputSelector InternalSelector;
		TSharedPtr<IPCGAttributeAccessor> InternalAccessor;

		const FPCGMetadataAttributeBase* Attribute = nullptr;
		EPCGExTransformComponent Component = EPCGExTransformComponent::Position;
		bool bUseAxis = false;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		EPCGExSingleField Field = EPCGExSingleField::X;

		bool ApplySelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
		{
			InternalSelector = InSelector.CopyAndFixLast(InData);
			bValid = InternalSelector.IsValid();

			if (!bValid) { return false; }

			const TArray<FString>& ExtraNames = InternalSelector.GetExtraNames();
			if (GetAxisSelection(ExtraNames, Axis))
			{
				bUseAxis = true;
				// An axis is set, treat as vector.
				// Only axis is set, assume rotation instead of position
				if (!GetComponentSelection(ExtraNames, Component)) { Component = EPCGExTransformComponent::Rotation; }
			}
			else
			{
				bUseAxis = false;
				GetComponentSelection(ExtraNames, Component);
			}

			GetFieldSelection(ExtraNames, Field);
			FullName = GetSelectorFullName<true>(InternalSelector, InData);

			if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				Attribute = nullptr;
				bValid = false;

				if (const UPCGSpatialData* AsSpatial = Cast<UPCGSpatialData>(InData))
				{
					Attribute = AsSpatial->Metadata->GetConstAttribute(InternalSelector.GetAttributeName());
					if (Attribute)
					{
						Attribute->GetTypeId(),
							PCGEx::ExecuteWithRightType(
								Attribute->GetTypeId(), [&](auto DummyValue)
								{
									using RawT = decltype(DummyValue);
									InternalAccessor = MakeShared<FPCGAttributeAccessor<RawT>>(static_cast<const FPCGMetadataAttribute<RawT>*>(Attribute), AsSpatial->Metadata);
								});

						bValid = true;
					}
				}
			}

			return bValid;
		}

	public:
		TAttributeBroadcaster()
		{
		}

		FString GetName() { return InternalSelector.GetName().ToString(); }

		virtual EPCGMetadataTypes GetType() const { return GetMetadataType<T>(); }
		const FPCGMetadataAttributeBase* GetAttribute() const { return Attribute; }

		FName FullName = NAME_None;

		TArray<T> Values;
		mutable T Min = T{};
		mutable T Max = T{};

		bool bEnabled = true;
		bool bValid = false;

		bool IsUsable(int32 NumEntries) { return bValid && Values.Num() >= NumEntries; }

		bool Prepare(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			ResetMinMax();

			bMinMaxDirty = true;
			bNormalized = false;
			PointIO = InPointIO;

			return ApplySelector(InSelector, InPointIO->GetIn());
		}

		bool Prepare(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			FPCGAttributePropertyInputSelector InSelector = FPCGAttributePropertyInputSelector();
			InSelector.Update(InName.ToString());
			return Prepare(InSelector, InPointIO);
		}

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param Dump
		 */
		void Fetch(TArray<T>& Dump, const PCGExMT::FScope& Scope)
		{
			check(bValid)
			check(Dump.Num() == PointIO->GetNum(PCGExData::ESource::In)) // Dump target should be initialized at full length before using Fetch

			const UPCGPointData* InData = PointIO->GetIn();

			if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				PCGEx::ExecuteWithRightType(
					Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);

						TArray<RawT> RawValues;
						PCGEx::InitArray(RawValues, Scope.Count);

						TArrayView<RawT> RawView(RawValues);
						InternalAccessor->GetRange(RawView, Scope.Start, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						for (int i = 0; i < Scope.Count; i++) { Dump[Scope.Start + i] = Convert(RawValues[i]); }
					});
			}
			else if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
			{
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: for (int i = Scope.Start; i < Scope.End; i++) { Dump[i] = Convert(InPoints[i]._ACCESSOR); } break;

				switch (InternalSelector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
			{
				switch (InternalSelector.GetExtraProperty())
				{
				case EPCGExtraProperties::Index:
					for (int i = Scope.Start; i < Scope.End; i++) { Dump[i] = Convert(i); }
					break;
				default: ;
				}
			}
		}

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param Dump
		 * @param bCaptureMinMax
		 * @param OutMin
		 * @param OutMax
		 */
		void GrabAndDump(TArray<T>& Dump, const bool bCaptureMinMax, T& OutMin, T& OutMax)
		{
			if (!bValid)
			{
				int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
				PCGEx::InitArray(Dump, NumPoints);
				for (int i = 0; i < NumPoints; i++) { Dump[i] = T{}; }
				return;
			}

			const UPCGPointData* InData = PointIO->GetIn();

			int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
			PCGEx::InitArray(Dump, NumPoints);

			if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				PCGEx::ExecuteWithRightType(
					Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);
						TArray<RawT> RawValues;

						PCGEx::InitArray(RawValues, NumPoints);

						TArrayView<RawT> View(RawValues);
						InternalAccessor->GetRange(View, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						if (bCaptureMinMax)
						{
							for (int i = 0; i < NumPoints; i++)
							{
								T V = Convert(RawValues[i]);
								OutMin = PCGExBlend::Min(V, OutMin);
								OutMax = PCGExBlend::Max(V, OutMax);
								Dump[i] = V;
							}
						}
						else
						{
							for (int i = 0; i < NumPoints; i++) { Dump[i] = Convert(RawValues[i]); }
						}

						RawValues.Empty();
					});
			}
			else if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
			{
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();

#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM:\
				if (bCaptureMinMax) { for (int i = 0; i < NumPoints; i++) {\
						T V = Convert(InPoints[i]._ACCESSOR); OutMin = PCGExBlend::Min(V, OutMin); OutMax = PCGExBlend::Max(V, OutMax); Dump[i] = V;\
					} } else { for (int i = 0; i < NumPoints; i++) { Dump[i] = Convert(InPoints[i]._ACCESSOR); } } break;

				switch (InternalSelector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
			{
				switch (InternalSelector.GetExtraProperty())
				{
				case EPCGExtraProperties::Index:
					for (int i = 0; i < NumPoints; i++) { Dump[i] = Convert(i); }
					break;
				default: ;
				}
			}
		}

		void GrabUniqueValues(TSet<T>& Dump)
		{
			if (!bValid) { return; }

			const UPCGPointData* InData = PointIO->GetIn();

			int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
			Dump.Reserve(Dump.Num() + NumPoints);

			if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				PCGEx::ExecuteWithRightType(
					Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);
						TArray<RawT> RawValues;

						PCGEx::InitArray(RawValues, NumPoints);

						TArrayView<RawT> View(RawValues);
						InternalAccessor->GetRange(View, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						for (int i = 0; i < NumPoints; i++) { Dump.Add(Convert(RawValues[i])); }

						RawValues.Empty();
					});
			}
			else if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
			{
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();

#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: for (int i = 0; i < NumPoints; i++) { Dump.Add(Convert(InPoints[i]._ACCESSOR)); } break;
				switch (InternalSelector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
			{
				switch (InternalSelector.GetExtraProperty())
				{
				case EPCGExtraProperties::Index:
					for (int i = 0; i < NumPoints; i++) { Dump.Add(Convert(i)); }
					break;
				default: ;
				}
			}

			Dump.Shrink();
		}

		void Grab(const bool bCaptureMinMax = false)
		{
			GrabAndDump(Values, bCaptureMinMax, Min, Max);
		}

		void UpdateMinMax()
		{
			if (!bMinMaxDirty) { return; }
			ResetMinMax();
			bMinMaxDirty = false;
			for (int i = 0; i < Values.Num(); i++)
			{
				T V = Values[i];
				Min = PCGExBlend::Min(V, Min);
				Max = PCGExBlend::Max(V, Max);
			}
		}

		void Normalize()
		{
			if (bNormalized) { return; }
			bNormalized = true;
			UpdateMinMax();
			T Range = PCGExBlend::Sub(Max, Min);
			for (int i = 0; i < Values.Num(); i++) { Values[i] = PCGExBlend::Div(Values[i], Range); }
		}

		T SoftGet(const int32 Index, const FPCGPoint& Point, const T& Fallback) const
		{
			if (!bValid) { return Fallback; }
			switch (InternalSelector.GetSelection())
			{
			case EPCGAttributePropertySelection::Attribute:
				return PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> T
					{
						using RawT = decltype(DummyValue);
						const FPCGMetadataAttribute<RawT>* TypedAttribute = static_cast<const FPCGMetadataAttribute<RawT>*>(Attribute);
						return Convert(TypedAttribute->GetValueFromItemKey(Point.MetadataEntry));
					});
			case EPCGAttributePropertySelection::PointProperty:
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: return Convert(Point._ACCESSOR); break;
				switch (InternalSelector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			case EPCGAttributePropertySelection::ExtraProperty:
				switch (InternalSelector.GetExtraProperty())
				{
				case EPCGExtraProperties::Index:
					return Convert(Index);
				default: ;
				}
			default:
				return Fallback;
			}
		}

		T SoftGet(const PCGExData::FPointRef& PointRef, const T& Fallback) const { return SoftGet(PointRef.Index, *PointRef.Point, Fallback); }

		T SafeGet(const int32 Index, const T& Fallback) const { return !bValid ? Fallback : Values[Index]; }
		T operator[](int32 Index) const { return bValid ? Values[Index] : T{}; }

		static TSharedPtr<TAttributeBroadcaster<T>> Make(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<T>)
			if (!Broadcaster->Prepare(InName, InPointIO)) { return nullptr; }
			return Broadcaster;
		}

		static TSharedPtr<TAttributeBroadcaster<T>> Make(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<T>)
			if (!Broadcaster->Prepare(InSelector, InPointIO)) { return nullptr; }
			return Broadcaster;
		}

	protected:
		virtual void ResetMinMax() { PCGExMath::TypeMinMax(Min, Max); }

#pragma region Conversions

#pragma region Convert from bool

		virtual T Convert(const bool Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value ? 1 : 0; }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value ? 1 : 0); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value ? 1 : 0); }
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				const double D = Value ? 1 : 0;
				return FVector4(D, D, D, D);
			}
			else if constexpr (std::is_same_v<T, FQuat>)
			{
				const double D = Value ? 180 : 0;
				return FRotator(D, D, D).Quaternion();
			}
			else if constexpr (std::is_same_v<T, FRotator>)
			{
				const double D = Value ? 180 : 0;
				return FRotator(D, D, D);
			}
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false")); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false"))); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Integer32

		virtual T Convert(const int32 Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%d"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%d"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Integer64

		virtual T Convert(const int64 Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%lld"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%lld)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Float

		virtual T Convert(const float Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%f"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%f)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Double

		virtual T Convert(const double Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%f"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%f)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector2D

		virtual T Convert(const FVector2D& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X > 0;
				case EPCGExSingleField::Y:
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Y > 0;
				case EPCGExSingleField::Length:
				case EPCGExSingleField::SquaredLength:
				case EPCGExSingleField::Volume:
					return Value.SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X;
				case EPCGExSingleField::Y:
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Y;
				case EPCGExSingleField::Length:
					return Value.Length();
				case EPCGExSingleField::SquaredLength:
					return Value.SquaredLength();
				case EPCGExSingleField::Volume:
					return Value.X * Value.Y;
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, 0, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, 0).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector

		virtual T Convert(const FVector& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X > 0;
				case EPCGExSingleField::Y:
					return Value.Y > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Z > 0;
				case EPCGExSingleField::Length:
				case EPCGExSingleField::SquaredLength:
				case EPCGExSingleField::Volume:
					return Value.SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X;
				case EPCGExSingleField::Y:
					return Value.Y;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Z;
				case EPCGExSingleField::Length:
					return Value.Length();
				case EPCGExSingleField::SquaredLength:
					return Value.SquaredLength();
				case EPCGExSingleField::Volume:
					return Value.X * Value.Y * Value.Z;
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); /* TODO : Handle axis selection */ }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis selection */ }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector4

		virtual T Convert(const FVector4& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X > 0;
				case EPCGExSingleField::Y:
					return Value.Y > 0;
				case EPCGExSingleField::Z:
					return Value.Z > 0;
				case EPCGExSingleField::W:
					return Value.W > 0;
				case EPCGExSingleField::Length:
				case EPCGExSingleField::SquaredLength:
				case EPCGExSingleField::Volume:
					return FVector(Value).SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X;
				case EPCGExSingleField::Y:
					return Value.Y;
				case EPCGExSingleField::Z:
					return Value.Z;
				case EPCGExSingleField::W:
					return Value.W;
				case EPCGExSingleField::Length:
					return FVector(Value).Length();
				case EPCGExSingleField::SquaredLength:
					return FVector(Value).SquaredLength();
				case EPCGExSingleField::Volume:
					return Value.X * Value.Y * Value.Z * Value.W;
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, Value.W); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis */ }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FQuat

		virtual T Convert(const FQuat& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>)
			{
				const FVector Dir = PCGExMath::GetDirection(Value, Axis);
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Dir.X > 0;
				case EPCGExSingleField::Y:
					return Dir.Y > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Dir.Z > 0;
				case EPCGExSingleField::Length:
				case EPCGExSingleField::SquaredLength:
				case EPCGExSingleField::Volume:
					return Dir.SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				const FVector Dir = PCGExMath::GetDirection(Value, Axis);
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Dir.X;
				case EPCGExSingleField::Y:
					return Dir.Y;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Dir.Z;
				case EPCGExSingleField::Length:
					return Dir.Length();
				case EPCGExSingleField::SquaredLength:
				case EPCGExSingleField::Volume:
					return Dir.SquaredLength();
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>)
			{
				const FVector Dir = PCGExMath::GetDirection(Value, Axis);
				return FVector2D(Dir.X, Dir.Y);
			}
			else if constexpr (std::is_same_v<T, FVector>) { return PCGExMath::GetDirection(Value, Axis); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(PCGExMath::GetDirection(Value, Axis), 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return Value; }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value.Rotator(); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value, FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FRotator

		virtual T Convert(const FRotator& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(FVector(Value.Pitch, Value.Roll, Value.Yaw)); }

			else if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.Pitch > 0;
				case EPCGExSingleField::Y:
					return Value.Yaw > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Roll > 0;
				case EPCGExSingleField::Length:
				case EPCGExSingleField::SquaredLength:
				case EPCGExSingleField::Volume:
					return Value.Euler().SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.Pitch > 0;
				case EPCGExSingleField::Y:
					return Value.Yaw > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Roll > 0;
				case EPCGExSingleField::Length:
					return Value.Euler().Length();
				case EPCGExSingleField::SquaredLength:
				case EPCGExSingleField::Volume:
					return Value.Euler().SquaredLength();
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return Convert(Value.Quaternion()); }
			else if constexpr (std::is_same_v<T, FVector>) { return Convert(Value.Quaternion()); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.Euler(), 0); /* TODO : Handle axis */ }
			else if constexpr (std::is_same_v<T, FQuat>) { return Value.Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value; }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value.Quaternion(), FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FTransform

		virtual T Convert(const FTransform& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (
				std::is_same_v<T, bool> ||
				std::is_same_v<T, int32> ||
				std::is_same_v<T, int64> ||
				std::is_same_v<T, float> ||
				std::is_same_v<T, double> ||
				std::is_same_v<T, FVector2D> ||
				std::is_same_v<T, FVector> ||
				std::is_same_v<T, FVector4> ||
				std::is_same_v<T, FQuat> ||
				std::is_same_v<T, FRotator>)
			{
				switch (Component)
				{
				default:
				case EPCGExTransformComponent::Position: return Convert(Value.GetLocation());
				case EPCGExTransformComponent::Rotation: return Convert(Value.GetRotation());
				case EPCGExTransformComponent::Scale: return Convert(Value.GetScale3D());
				}
			}
			else if constexpr (std::is_same_v<T, FTransform>) { return Value; }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FString

		virtual T Convert(const FString& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value; }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FName

		virtual T Convert(const FName& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return Value; }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FSoftClassPath

		virtual T Convert(const FSoftClassPath& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return Value; }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FSoftObjectPath

		virtual T Convert(const FSoftObjectPath& Value) const
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return Value; }
			else { return T{}; }
		}

#pragma endregion

#pragma endregion
	};

#pragma endregion

#pragma region Attribute copy

	void CopyPoints(
		const PCGExData::FPointIO* Source,
		const PCGExData::FPointIO* Target,
		const TSharedPtr<const TArray<int32>>& SourceIndices,
		const int32 TargetIndex = 0,
		const bool bKeepSourceMetadataEntry = false);

#pragma endregion

	TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError);
}

#undef PCGEX_AAFLAG
