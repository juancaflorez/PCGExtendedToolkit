﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExGraph.generated.h"

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExEdgeType : uint8
{
	Unknown  = 0 UMETA(DisplayName = "Unknown", Tooltip="Unknown type."),
	Roaming  = 1 << 0 UMETA(DisplayName = "Roaming", Tooltip="Unidirectional edge."),
	Shared   = 1 << 1 UMETA(DisplayName = "Shared", Tooltip="Shared edge, both sockets are connected; but do not match."),
	Match    = 1 << 2 UMETA(DisplayName = "Match", Tooltip="Shared relation, considered a match by the primary socket owner; but does not match on the other."),
	Complete = 1 << 3 UMETA(DisplayName = "Complete", Tooltip="Shared, matching relation on both sockets."),
	Mirror   = 1 << 4 UMETA(DisplayName = "Mirrored relation", Tooltip="Mirrored relation, connected sockets are the same on both points."),
};

ENUM_CLASS_FLAGS(EPCGExEdgeType)

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExTangentType : uint8
{
	Custom      = 0 UMETA(DisplayName = "Custom", Tooltip="Custom Attributes."),
	Extrapolate = 0 UMETA(DisplayName = "Extrapolate", Tooltip="Extrapolate from neighbors position and direction"),
};

ENUM_CLASS_FLAGS(EPCGExTangentType)

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExSocketType : uint8
{
	None   = 0 UMETA(DisplayName = "None", Tooltip="This socket has no particular type."),
	Output = 1 << 0 UMETA(DisplayName = "Output", Tooltip="This socket in an output socket. It can only connect to Input sockets."),
	Input  = 1 << 1 UMETA(DisplayName = "Input", Tooltip="This socket in an input socket. It can only connect to Output sockets"),
	Any    = Output | Input UMETA(DisplayName = "Any", Tooltip="This socket is considered both an Output and and Input."),
};

ENUM_CLASS_FLAGS(EPCGExSocketType)

#pragma region Descriptors

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketAngle
{
	GENERATED_BODY()

	FPCGExSocketAngle()
		: DotOverDistance(PCGEx::DefaultDotOverDistanceCurve)
	{
	}

	FPCGExSocketAngle(const FVector& Dir): FPCGExSocketAngle()
	{
		Direction = Dir;
	}

	FPCGExSocketAngle(
		const FPCGExSocketAngle& Other):
		Direction(Other.Direction),
		DotThreshold(Other.DotThreshold),
		MaxDistance(Other.MaxDistance),
		DotOverDistance(Other.DotOverDistance)
	{
	}

public:
	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Direction = FVector::UpVector;

	/** Cone threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(Units="Degrees", UIMin=0, UIMax=180))
	double Angle = 45.0; // 0.707f dot

	UPROPERTY(BlueprintReadOnly, meta=(UIMin=-1, UIMax=1))
	double DotThreshold = 0.707;

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double MaxDistance = 1000.0f;

	/** The balance over distance to prioritize closer distance or better alignment. Curve X is normalized distance; Y = 0 means narrower dot wins, Y = 1 means closer distance wins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UCurveFloat> DotOverDistance;

	UCurveFloat* DotOverDistanceCurve = nullptr;

	void LoadCurve() { DotOverDistanceCurve = DotOverDistance.LoadSynchronous(); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketDescriptor
{
	GENERATED_BODY()

	FPCGExSocketDescriptor()
	{
	}

	FPCGExSocketDescriptor(
		const FName InName,
		const FVector& InDirection,
		const EPCGExSocketType InType,
		const FColor InDebugColor,
		const double InAngle = 45):
		SocketName(InName),
		SocketType(InType),
		DebugColor(InDebugColor)
	{
		Angle.Direction = InDirection;
		Angle.Angle = InAngle;
	}

	FPCGExSocketDescriptor(
		const FName InName,
		const FVector& InDirection,
		const FName InMatchingSlot,
		const EPCGExSocketType InType,
		const FColor InDebugColor,
		const double InAngle = 45):
		SocketName(InName),
		SocketType(InType),
		DebugColor(InDebugColor)
	{
		Angle.Direction = InDirection;
		Angle.Angle = InAngle;
		MatchingSlots.Add(InMatchingSlot);
	}

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	FName SocketName = NAME_None;

	/** Type of socket. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	EPCGExSocketType SocketType = EPCGExSocketType::Any;

	/** Exclusive sockets can only connect to other socket matching */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	bool bExclusiveBehavior = false;

	/** Socket spatial definition */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FPCGExSocketAngle Angle;

	/** Whether the orientation of the direction is relative to the point transform or not. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	bool bRelativeOrientation = true;

	/** If true, the direction vector of the socket will be read from a local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bDirectionVectorFromAttribute = false;

	/** Sibling slots names that are to be considered as a match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> MatchingSlots;

	/** Inject this slot as a match to slots referenced in the Matching Slots list. Useful to save time */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bMirrorMatchingSockets = true;

	/** Local attribute to override the direction vector with */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDirectionVectorFromAttribute", ShowOnlyInnerProperties))
	FPCGExInputDescriptorWithDirection AttributeDirectionVector;

	/** If enabled, multiplies the max sampling distance of this socket by the value of a local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bApplyAttributeModifier = false;

	/** Local attribute to multiply the max distance by.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyAttributeModifier", ShowOnlyInnerProperties))
	FPCGExInputDescriptorWithSingleField AttributeModifier;

	/** Offset socket origin  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExExtension OffsetOrigin = EPCGExExtension::None;

	/** Enable/disable this socket. Disabled sockets are omitted during processing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	bool bEnabled = true;

	/** Debug color for arrows. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	FColor DebugColor = FColor::Red;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketGlobalOverrides
{
	GENERATED_BODY()

	FPCGExSocketGlobalOverrides()
		: DotOverDistance(PCGEx::DefaultDotOverDistanceCurve)
	{
	}

public:
	/** Override all socket orientation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideRelativeOrientation = false;

	/** If true, the direction vector will be affected by the point' world rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideRelativeOrientation"))
	bool bRelativeOrientation = false;


	/** Override all socket bExclusiveBehavior. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideExclusiveBehavior = false;

	/** If true, Input & Outputs can only connect to sockets registered as a Match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideExclusiveBehavior"))
	bool bExclusiveBehavior = false;


	/** Override all socket cone. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideAngle = false;

	/** Cone threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAngle"))
	double Angle = 45.0; // 0.707f dot


	/** Override all socket distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideMaxDistance = false;

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideMaxDistance"))
	double MaxDistance = 100.0f;


	/** Override all socket Read direction vector from local attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bOverrideDirectionVectorFromAttribute = false;

	/** Is the direction vector read from local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle, EditCondition="bOverrideDirectionVectorFromAttribute"))
	bool bDirectionVectorFromAttribute = false;

	/** Local attribute from which the direction will be read. Must be a FVectorN otherwise will lead to bad results. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideDirectionVectorFromAttribute", ShowOnlyInnerProperties))
	FPCGExInputDescriptor AttributeDirectionVector;


	/** Override all socket Modifiers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideAttributeModifier = false;

	/** Is the distance modified by local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAttributeModifier", InlineEditConditionToggle))
	bool bApplyAttributeModifier = false;

	/** Which local attribute is used to factor the distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAttributeModifier"))
	FPCGExInputDescriptorWithSingleField AttributeModifier;

	/** Is the distance modified by local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideDotOverDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideDotOverDistance"))
	TSoftObjectPtr<UCurveFloat> DotOverDistance;

	/** Offset socket origin */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideOffsetOrigin = false;

	/** Offset socket origin  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideOffsetOrigin"))
	EPCGExExtension OffsetOrigin = EPCGExExtension::None;
};

#pragma endregion

namespace PCGExGraph
{

	const FName SourceParamsLabel = TEXT("Graph");
	const FName OutputParamsLabel = TEXT("➜");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName OutputPatchesLabel = TEXT("Out");

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	constexpr PCGExMT::AsyncState State_ReadyForNextGraph = 100;
	constexpr PCGExMT::AsyncState State_ProcessingGraph = 101;

	constexpr PCGExMT::AsyncState State_CachingGraphIndices = 105;
	constexpr PCGExMT::AsyncState State_SwappingGraphIndices = 106;
	
	constexpr PCGExMT::AsyncState State_FindingEdgeTypes = 110;
	constexpr PCGExMT::AsyncState State_FindingPatch = 120;
	//constexpr PCGExMT::State PatchComputing = 130;
	
	
#pragma region Sockets

	struct PCGEXTENDEDTOOLKIT_API FSocketMetadata
	{
		FSocketMetadata()
		{
		}

		FSocketMetadata(int64 InIndex, PCGMetadataEntryKey InEntryKey, EPCGExEdgeType InEdgeType):
			Index(InIndex),
			EntryKey(InEntryKey),
			EdgeType(InEdgeType)
		{
		}

	public:
		int64 Index = -1; // Index of the point this socket connects to
		PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
		EPCGExEdgeType EdgeType = EPCGExEdgeType::Unknown;

		bool operator==(const FSocketMetadata& Other) const
		{
			return Index == Other.Index && EntryKey == Other.EntryKey && EdgeType == Other.EdgeType;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FProbeDistanceModifier : public PCGEx::FLocalSingleFieldGetter
	{
		FProbeDistanceModifier(): FLocalSingleFieldGetter()
		{
		}

		FProbeDistanceModifier(const FPCGExSocketDescriptor& InDescriptor): FLocalSingleFieldGetter()
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor.AttributeModifier);
			bEnabled = InDescriptor.bApplyAttributeModifier;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalDirection : public PCGEx::FLocalDirectionGetter
	{
		FLocalDirection(): FLocalDirectionGetter()
		{
		}

		FLocalDirection(const FPCGExSocketDescriptor& InDescriptor): FLocalDirectionGetter()
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor.AttributeDirectionVector);
			bEnabled = InDescriptor.bDirectionVectorFromAttribute;
		}
	};

	const FName SocketPropertyNameIndex = FName("Target");
	const FName SocketPropertyNameEdgeType = FName("EdgeType");
	const FName SocketPropertyNameEntryKey = FName("TargetEntryKey");

	struct PCGEXTENDEDTOOLKIT_API FSocket
	{
		FSocket()
		{
			MatchingSockets.Empty();
		}

		FSocket(const FPCGExSocketDescriptor& InDescriptor): FSocket()
		{
			Descriptor = InDescriptor;
			Descriptor.Angle.DotThreshold = FMath::Cos(Descriptor.Angle.Angle * (PI / 180.0)); //Degrees to dot product
		}

		friend struct FSocketMapping;
		FPCGExSocketDescriptor Descriptor;

		int32 SocketIndex = -1;
		TSet<int32> MatchingSockets;

	protected:
		FPCGMetadataAttribute<int64>* AttributeTargetIndex = nullptr;
		FPCGMetadataAttribute<int32>* AttributeEdgeType = nullptr;
		FPCGMetadataAttribute<int64>* AttributeTargetEntryKey = nullptr;
		FName AttributeNameBase = NAME_None;

	public:
		FName GetName() const { return AttributeNameBase; }
		EPCGExSocketType GetType() const { return Descriptor.SocketType; }
		bool IsExclusive() const { return Descriptor.bExclusiveBehavior; }
		bool Matches(const FSocket* OtherSocket) const { return MatchingSockets.Contains(OtherSocket->SocketIndex); }

		void DeleteFrom(const UPCGPointData* PointData) const
		{
			if (AttributeTargetIndex) { PointData->Metadata->DeleteAttribute(AttributeTargetIndex->Name); }
			if (AttributeEdgeType) { PointData->Metadata->DeleteAttribute(AttributeEdgeType->Name); }
			if (AttributeTargetEntryKey) { PointData->Metadata->DeleteAttribute(AttributeTargetEntryKey->Name); }
		}

		/**
		 * Find or create the attribute matching this socket on a given PointData object,
		 * as well as prepare the scape modifier for that same object.
		 * @param PointData
		 * @param bEnsureEdgeType 
		 */
		void PrepareForPointData(const UPCGPointData* PointData, const bool bEnsureEdgeType)
		{
			AttributeTargetIndex = GetAttribute(PointData, SocketPropertyNameIndex, true, static_cast<int64>(-1));
			AttributeTargetEntryKey = GetAttribute(PointData, SocketPropertyNameEntryKey, true, PCGInvalidEntryKey);
			AttributeEdgeType = GetAttribute(PointData, SocketPropertyNameEdgeType, bEnsureEdgeType, static_cast<int32>(EPCGExEdgeType::Unknown));
			Descriptor.Angle.LoadCurve();
		}

	protected:
		template <typename T>
		FPCGMetadataAttribute<T>* GetAttribute(const UPCGPointData* PointData, const FName PropertyName, bool bEnsureAttributeExists, T DefaultValue)
		{
			if (bEnsureAttributeExists || PointData->Metadata->HasAttribute(GetSocketPropertyName(PropertyName)))
			{
				return PointData->Metadata->FindOrCreateAttribute<T>(GetSocketPropertyName(PropertyName), DefaultValue, false);
			}

			return nullptr;
		}

	public:
		void SetData(const PCGMetadataEntryKey MetadataEntry, const FSocketMetadata& SocketMetadata) const
		{
			SetTargetIndex(MetadataEntry, SocketMetadata.Index);
			SetEdgeType(MetadataEntry, SocketMetadata.EdgeType);
		}

		// Point index within the same data group.
		void SetTargetIndex(const PCGMetadataEntryKey MetadataEntry, int64 InIndex) const { AttributeTargetIndex->SetValue(MetadataEntry, InIndex); }
		int64 GetTargetIndex(const PCGMetadataEntryKey MetadataEntry) const { return AttributeTargetIndex->GetValueFromItemKey(MetadataEntry); }

		// Point metadata entry key, faster than retrieving index if you only need to access attributes
		void SetTargetEntryKey(const PCGMetadataEntryKey MetadataEntry, PCGMetadataEntryKey InEntryKey) const { AttributeTargetEntryKey->SetValue(MetadataEntry, InEntryKey); }
		PCGMetadataEntryKey GetTargetEntryKey(const PCGMetadataEntryKey MetadataEntry) const { return AttributeTargetEntryKey->GetValueFromItemKey(MetadataEntry); }

		// Relation type
		void SetEdgeType(const PCGMetadataEntryKey MetadataEntry, EPCGExEdgeType InEdgeType) const
		{
			if (!AttributeEdgeType) { return; }
			AttributeEdgeType->SetValue(MetadataEntry, static_cast<int32>(InEdgeType));
		}

		EPCGExEdgeType GetEdgeType(const PCGMetadataEntryKey MetadataEntry) const
		{
			return AttributeEdgeType ? static_cast<EPCGExEdgeType>(AttributeEdgeType->GetValueFromItemKey(MetadataEntry)) : EPCGExEdgeType::Unknown;
		}

		FSocketMetadata GetData(const PCGMetadataEntryKey MetadataEntry) const
		{
			return FSocketMetadata(
					GetTargetIndex(MetadataEntry),
					GetTargetEntryKey(MetadataEntry),
					GetEdgeType(MetadataEntry)
				);
		}

		template <typename T>
		bool TryGetEdge(int64 Start, const PCGMetadataEntryKey MetadataEntry, T& OutEdge) const
		{
			const int64 End = GetTargetIndex(MetadataEntry);
			if (End == -1) { return false; }
			OutEdge = T(
				Start,
				End,
				GetEdgeType(MetadataEntry));
			return true;
		}

		FName GetSocketPropertyName(FName PropertyName) const
		{
			const FString Separator = TEXT("/");
			return *(AttributeNameBase.ToString() + Separator + PropertyName.ToString());
		}

	public:
		~FSocket()
		{
			AttributeTargetIndex = nullptr;
			AttributeEdgeType = nullptr;
			AttributeTargetEntryKey = nullptr;
			MatchingSockets.Empty();
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FSocketInfos
	{
		FSocket* Socket = nullptr;
		FProbeDistanceModifier* Modifier = nullptr;
		FLocalDirection* LocalDirection = nullptr;

		bool Matches(const FSocketInfos& Other) const { return Socket->Matches(Other.Socket); }
	};

	struct PCGEXTENDEDTOOLKIT_API FSocketMapping
	{
		FSocketMapping()
		{
			Reset();
		}

	public:
		FName Identifier = NAME_None;
		TArray<FSocket> Sockets;
		TArray<FProbeDistanceModifier> Modifiers;
		TArray<FLocalDirection> LocalDirections;
		TMap<FName, int32> NameToIndexMap;
		int32 NumSockets = 0;

		void Initialize(const FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets)
		{
			Reset();
			Identifier = InIdentifier;
			for (FPCGExSocketDescriptor& Descriptor : InSockets)
			{
				if (!Descriptor.bEnabled) { continue; }

				FProbeDistanceModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
				FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);

				FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
				NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
				NewSocket.SocketIndex = NumSockets;
				NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);
				NumSockets++;
			}

			PostProcessSockets();
		}

		void InitializeWithOverrides(FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets, const FPCGExSocketGlobalOverrides& Overrides)
		{
			Reset();
			Identifier = InIdentifier;
			const FString PCGExName = TEXT("PCGEx");
			for (FPCGExSocketDescriptor& Descriptor : InSockets)
			{
				if (!Descriptor.bEnabled) { continue; }

				FProbeDistanceModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
				NewModifier.bEnabled = Overrides.bOverrideAttributeModifier ? Overrides.bApplyAttributeModifier : Descriptor.bApplyAttributeModifier;
				NewModifier.Descriptor = static_cast<FPCGExInputDescriptor>(Overrides.bOverrideAttributeModifier ? Overrides.AttributeModifier : Descriptor.AttributeModifier);

				FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);
				NewLocalDirection.bEnabled = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.bDirectionVectorFromAttribute : Descriptor.bDirectionVectorFromAttribute;
				NewLocalDirection.Descriptor = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.AttributeDirectionVector : Descriptor.AttributeDirectionVector;

				FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
				NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
				NewSocket.SocketIndex = NumSockets;
				NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);

				if (Overrides.bOverrideRelativeOrientation) { NewSocket.Descriptor.bRelativeOrientation = Overrides.bRelativeOrientation; }
				if (Overrides.bOverrideAngle) { NewSocket.Descriptor.Angle.Angle = Overrides.Angle; }
				if (Overrides.bOverrideMaxDistance) { NewSocket.Descriptor.Angle.MaxDistance = Overrides.MaxDistance; }
				if (Overrides.bOverrideExclusiveBehavior) { NewSocket.Descriptor.bExclusiveBehavior = Overrides.bExclusiveBehavior; }
				if (Overrides.bOverrideDotOverDistance) { NewSocket.Descriptor.Angle.DotOverDistance = Overrides.DotOverDistance; }
				if (Overrides.bOverrideOffsetOrigin) { NewSocket.Descriptor.OffsetOrigin = Overrides.OffsetOrigin; }

				NewSocket.Descriptor.Angle.DotThreshold = FMath::Cos(NewSocket.Descriptor.Angle.Angle * (PI / 180.0));

				NumSockets++;
			}

			PostProcessSockets();
		}

		FName GetCompoundName(FName SecondaryIdentifier) const
		{
			const FString Separator = TEXT("/");
			return *(TEXT("PCGEx") + Separator + Identifier.ToString() + Separator + SecondaryIdentifier.ToString()); // PCGEx/ParamsIdentifier/SocketIdentifier
		}

		/**
		 * Prepare socket mapping for working with a given PointData object.
		 * Each socket will cache Attribute & accessors
		 * @param PointData
		 * @param bEnsureEdgeType Whether EdgeType attribute must be created if it doesn't exist. Set this to true if you intend on updating it. 
		 */
		void PrepareForPointData(const UPCGPointData* PointData, const bool bEnsureEdgeType)
		{
			for (int i = 0; i < Sockets.Num(); i++)
			{
				Sockets[i].PrepareForPointData(PointData, bEnsureEdgeType);
				Modifiers[i].Validate(PointData);
				LocalDirections[i].Validate(PointData);
			}
		}

		const TArray<FSocket>& GetSockets() const { return Sockets; }
		const TArray<FProbeDistanceModifier>& GetModifiers() const { return Modifiers; }

		void GetSocketsInfos(TArray<FSocketInfos>& OutInfos)
		{
			OutInfos.Empty(NumSockets);
			for (int i = 0; i < NumSockets; i++)
			{
				FSocketInfos& Infos = OutInfos.Emplace_GetRef();
				Infos.Socket = &(Sockets[i]);
				Infos.Modifier = &(Modifiers[i]);
				Infos.LocalDirection = &(LocalDirections[i]);
			}
		}

		void Reset()
		{
			Sockets.Empty();
			Modifiers.Empty();
			LocalDirections.Empty();
		}

	private:
		/**
		 * Build matching set
		 */
		void PostProcessSockets()
		{
			for (FSocket& Socket : Sockets)
			{
				for (const FName MatchingSocketName : Socket.Descriptor.MatchingSlots)
				{
					FName OtherSocketName = GetCompoundName(MatchingSocketName);
					if (const int32* Index = NameToIndexMap.Find(OtherSocketName))
					{
						Socket.MatchingSockets.Add(*Index);
						if (Socket.Descriptor.bMirrorMatchingSockets) { Sockets[*Index].MatchingSockets.Add(Socket.SocketIndex); }
					}
				}
			}
		}

	public:
		~FSocketMapping()
		{
			Reset();
		}
	};

#pragma endregion

#pragma region Edges

	struct PCGEXTENDEDTOOLKIT_API FEdge
	{
		uint32 Start = 0;
		uint32 End = 0;
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;

		FEdge()
		{
		}

		FEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			Start(InStart), End(InEnd), Type(InType)
		{
		}

		bool operator==(const FEdge& Other) const
		{
			return Start == Other.Start && End == Other.End;
		}

		explicit operator uint64() const
		{
			return (static_cast<uint64>(Start) << 32) | End;
		}

		explicit FEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			End = static_cast<uint32>(InValue & 0xFFFFFFFF);
			// You might need to set a default value for Type based on your requirements.
			Type = EPCGExEdgeType::Unknown;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FUnsignedEdge : public FEdge
	{
		FUnsignedEdge(): FEdge()
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			FEdge(InStart, InEnd, InType)
		{
		}

		bool operator==(const FUnsignedEdge& Other) const
		{
			return GetUnsignedHash() == Other.GetUnsignedHash();
		}

		explicit FUnsignedEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			End = static_cast<uint32>(InValue & 0xFFFFFFFF);
			// You might need to set a default value for Type based on your requirements.
			Type = EPCGExEdgeType::Unknown;
		}

		uint64 GetUnsignedHash() const
		{
			return Start > End ?
				       (static_cast<uint64>(Start) << 32) | End :
				       (static_cast<uint64>(End) << 32) | Start;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FCachedSocketData
	{
		FCachedSocketData()
		{
			Neighbors.Empty();
		}

		int64 Index = -1;
		TArray<FSocketMetadata> Neighbors;
	};

	/**
	 * Assume the edge already is neither None nor Unique, since another socket has been found.
	 * @param StartSocket 
	 * @param EndSocket 
	 * @return 
	 */
	static EPCGExEdgeType GetEdgeType(const FSocketInfos& StartSocket, const FSocketInfos& EndSocket)
	{
		if (StartSocket.Matches(EndSocket))
		{
			if (EndSocket.Matches(StartSocket))
			{
				return EPCGExEdgeType::Complete;
			}
			return EPCGExEdgeType::Match;
		}
		if (StartSocket.Socket->SocketIndex == EndSocket.Socket->SocketIndex)
		{
			// We check for mirror AFTER checking for shared/match, since Mirror can be considered a legal match by design
			// in which case we don't want to flag this as Mirrored.
			return EPCGExEdgeType::Mirror;
		}
		return EPCGExEdgeType::Shared;
	}

	static void ComputeEdgeType(
		const TArray<PCGExGraph::FSocketInfos>& SocketInfos,
		const FPCGPoint& Point,
		const int32 ReadIndex,
		const UPCGExPointIO* PointIO)
	{
		for (const PCGExGraph::FSocketInfos& CurrentSocketInfos : SocketInfos)
		{
			EPCGExEdgeType Type = EPCGExEdgeType::Unknown;
			const int64 RelationIndex = CurrentSocketInfos.Socket->GetTargetIndex(Point.MetadataEntry);

			if (RelationIndex != -1)
			{
				const int32 Key = PointIO->GetOutPoint(RelationIndex).MetadataEntry;
				for (const PCGExGraph::FSocketInfos& OtherSocketInfos : SocketInfos)
				{
					if (OtherSocketInfos.Socket->GetTargetIndex(Key) == ReadIndex)
					{
						Type = GetEdgeType(CurrentSocketInfos, OtherSocketInfos);
					}
				}

				if (Type == EPCGExEdgeType::Unknown) { Type = EPCGExEdgeType::Roaming; }
			}


			CurrentSocketInfos.Socket->SetEdgeType(Point.MetadataEntry, Type);
		}
	}

#pragma endregion
	
}