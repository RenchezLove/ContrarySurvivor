// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameAnalytics/Public/GAEnums.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeGAEnums() {}

// Begin Cross Module References
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdAction();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdError();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdType();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAValueType();
UPackage* Z_Construct_UPackage__Script_GameAnalytics();
// End Cross Module References

// Begin Enum EGAResourceFlowType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EGAResourceFlowType;
static UEnum* EGAResourceFlowType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EGAResourceFlowType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EGAResourceFlowType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("EGAResourceFlowType"));
	}
	return Z_Registration_Info_UEnum_EGAResourceFlowType.OuterSingleton;
}
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAResourceFlowType>()
{
	return EGAResourceFlowType_StaticEnum();
}
struct Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/GAEnums.h" },
		{ "sink.Name", "EGAResourceFlowType::sink" },
		{ "source.Name", "EGAResourceFlowType::source" },
		{ "undefined.Name", "EGAResourceFlowType::undefined" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGAResourceFlowType::undefined", (int64)EGAResourceFlowType::undefined },
		{ "EGAResourceFlowType::source", (int64)EGAResourceFlowType::source },
		{ "EGAResourceFlowType::sink", (int64)EGAResourceFlowType::sink },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	"EGAResourceFlowType",
	"EGAResourceFlowType",
	Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType()
{
	if (!Z_Registration_Info_UEnum_EGAResourceFlowType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EGAResourceFlowType.InnerSingleton, Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EGAResourceFlowType.InnerSingleton;
}
// End Enum EGAResourceFlowType

// Begin Enum EGAProgressionStatus
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EGAProgressionStatus;
static UEnum* EGAProgressionStatus_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EGAProgressionStatus.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EGAProgressionStatus.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("EGAProgressionStatus"));
	}
	return Z_Registration_Info_UEnum_EGAProgressionStatus.OuterSingleton;
}
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAProgressionStatus>()
{
	return EGAProgressionStatus_StaticEnum();
}
struct Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "complete.Name", "EGAProgressionStatus::complete" },
		{ "fail.Name", "EGAProgressionStatus::fail" },
		{ "ModuleRelativePath", "Public/GAEnums.h" },
		{ "start.Name", "EGAProgressionStatus::start" },
		{ "undefined.Name", "EGAProgressionStatus::undefined" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGAProgressionStatus::undefined", (int64)EGAProgressionStatus::undefined },
		{ "EGAProgressionStatus::start", (int64)EGAProgressionStatus::start },
		{ "EGAProgressionStatus::complete", (int64)EGAProgressionStatus::complete },
		{ "EGAProgressionStatus::fail", (int64)EGAProgressionStatus::fail },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	"EGAProgressionStatus",
	"EGAProgressionStatus",
	Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus_Statics::Enum_MetaDataParams), Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus()
{
	if (!Z_Registration_Info_UEnum_EGAProgressionStatus.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EGAProgressionStatus.InnerSingleton, Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EGAProgressionStatus.InnerSingleton;
}
// End Enum EGAProgressionStatus

// Begin Enum EGAErrorSeverity
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EGAErrorSeverity;
static UEnum* EGAErrorSeverity_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EGAErrorSeverity.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EGAErrorSeverity.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("EGAErrorSeverity"));
	}
	return Z_Registration_Info_UEnum_EGAErrorSeverity.OuterSingleton;
}
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAErrorSeverity>()
{
	return EGAErrorSeverity_StaticEnum();
}
struct Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "critical.Name", "EGAErrorSeverity::critical" },
		{ "debug.Name", "EGAErrorSeverity::debug" },
		{ "error.Name", "EGAErrorSeverity::error" },
		{ "info.Name", "EGAErrorSeverity::info" },
		{ "ModuleRelativePath", "Public/GAEnums.h" },
		{ "undefined.Name", "EGAErrorSeverity::undefined" },
		{ "warning.Name", "EGAErrorSeverity::warning" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGAErrorSeverity::undefined", (int64)EGAErrorSeverity::undefined },
		{ "EGAErrorSeverity::debug", (int64)EGAErrorSeverity::debug },
		{ "EGAErrorSeverity::info", (int64)EGAErrorSeverity::info },
		{ "EGAErrorSeverity::warning", (int64)EGAErrorSeverity::warning },
		{ "EGAErrorSeverity::error", (int64)EGAErrorSeverity::error },
		{ "EGAErrorSeverity::critical", (int64)EGAErrorSeverity::critical },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	"EGAErrorSeverity",
	"EGAErrorSeverity",
	Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity_Statics::Enum_MetaDataParams), Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity()
{
	if (!Z_Registration_Info_UEnum_EGAErrorSeverity.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EGAErrorSeverity.InnerSingleton, Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EGAErrorSeverity.InnerSingleton;
}
// End Enum EGAErrorSeverity

// Begin Enum EGAAdAction
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EGAAdAction;
static UEnum* EGAAdAction_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EGAAdAction.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EGAAdAction.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameAnalytics_EGAAdAction, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("EGAAdAction"));
	}
	return Z_Registration_Info_UEnum_EGAAdAction.OuterSingleton;
}
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAAdAction>()
{
	return EGAAdAction_StaticEnum();
}
struct Z_Construct_UEnum_GameAnalytics_EGAAdAction_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "clicked.Name", "EGAAdAction::clicked" },
		{ "failedshow.Name", "EGAAdAction::failedshow" },
		{ "loaded.Name", "EGAAdAction::loaded" },
		{ "ModuleRelativePath", "Public/GAEnums.h" },
		{ "request.Name", "EGAAdAction::request" },
		{ "rewardreceived.Name", "EGAAdAction::rewardreceived" },
		{ "show.Name", "EGAAdAction::show" },
		{ "undefined.Name", "EGAAdAction::undefined" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGAAdAction::undefined", (int64)EGAAdAction::undefined },
		{ "EGAAdAction::clicked", (int64)EGAAdAction::clicked },
		{ "EGAAdAction::show", (int64)EGAAdAction::show },
		{ "EGAAdAction::failedshow", (int64)EGAAdAction::failedshow },
		{ "EGAAdAction::rewardreceived", (int64)EGAAdAction::rewardreceived },
		{ "EGAAdAction::request", (int64)EGAAdAction::request },
		{ "EGAAdAction::loaded", (int64)EGAAdAction::loaded },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_GameAnalytics_EGAAdAction_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	"EGAAdAction",
	"EGAAdAction",
	Z_Construct_UEnum_GameAnalytics_EGAAdAction_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAAdAction_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAAdAction_Statics::Enum_MetaDataParams), Z_Construct_UEnum_GameAnalytics_EGAAdAction_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdAction()
{
	if (!Z_Registration_Info_UEnum_EGAAdAction.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EGAAdAction.InnerSingleton, Z_Construct_UEnum_GameAnalytics_EGAAdAction_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EGAAdAction.InnerSingleton;
}
// End Enum EGAAdAction

// Begin Enum EGAAdType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EGAAdType;
static UEnum* EGAAdType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EGAAdType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EGAAdType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameAnalytics_EGAAdType, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("EGAAdType"));
	}
	return Z_Registration_Info_UEnum_EGAAdType.OuterSingleton;
}
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAAdType>()
{
	return EGAAdType_StaticEnum();
}
struct Z_Construct_UEnum_GameAnalytics_EGAAdType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "banner.Name", "EGAAdType::banner" },
		{ "interstitial.Name", "EGAAdType::interstitial" },
		{ "ModuleRelativePath", "Public/GAEnums.h" },
		{ "offerwall.Name", "EGAAdType::offerwall" },
		{ "playable.Name", "EGAAdType::playable" },
		{ "rewardedvideo.Name", "EGAAdType::rewardedvideo" },
		{ "undefined.Name", "EGAAdType::undefined" },
		{ "video.Name", "EGAAdType::video" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGAAdType::undefined", (int64)EGAAdType::undefined },
		{ "EGAAdType::video", (int64)EGAAdType::video },
		{ "EGAAdType::rewardedvideo", (int64)EGAAdType::rewardedvideo },
		{ "EGAAdType::playable", (int64)EGAAdType::playable },
		{ "EGAAdType::interstitial", (int64)EGAAdType::interstitial },
		{ "EGAAdType::offerwall", (int64)EGAAdType::offerwall },
		{ "EGAAdType::banner", (int64)EGAAdType::banner },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_GameAnalytics_EGAAdType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	"EGAAdType",
	"EGAAdType",
	Z_Construct_UEnum_GameAnalytics_EGAAdType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAAdType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAAdType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_GameAnalytics_EGAAdType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdType()
{
	if (!Z_Registration_Info_UEnum_EGAAdType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EGAAdType.InnerSingleton, Z_Construct_UEnum_GameAnalytics_EGAAdType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EGAAdType.InnerSingleton;
}
// End Enum EGAAdType

// Begin Enum EGAAdError
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EGAAdError;
static UEnum* EGAAdError_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EGAAdError.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EGAAdError.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameAnalytics_EGAAdError, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("EGAAdError"));
	}
	return Z_Registration_Info_UEnum_EGAAdError.OuterSingleton;
}
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAAdError>()
{
	return EGAAdError_StaticEnum();
}
struct Z_Construct_UEnum_GameAnalytics_EGAAdError_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "internalerror.Name", "EGAAdError::internalerror" },
		{ "invalidrequest.Name", "EGAAdError::invalidrequest" },
		{ "ModuleRelativePath", "Public/GAEnums.h" },
		{ "nofill.Name", "EGAAdError::nofill" },
		{ "offline.Name", "EGAAdError::offline" },
		{ "unabletoprecache.Name", "EGAAdError::unabletoprecache" },
		{ "undefined.Name", "EGAAdError::undefined" },
		{ "unknown.Name", "EGAAdError::unknown" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGAAdError::undefined", (int64)EGAAdError::undefined },
		{ "EGAAdError::unknown", (int64)EGAAdError::unknown },
		{ "EGAAdError::offline", (int64)EGAAdError::offline },
		{ "EGAAdError::nofill", (int64)EGAAdError::nofill },
		{ "EGAAdError::internalerror", (int64)EGAAdError::internalerror },
		{ "EGAAdError::invalidrequest", (int64)EGAAdError::invalidrequest },
		{ "EGAAdError::unabletoprecache", (int64)EGAAdError::unabletoprecache },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_GameAnalytics_EGAAdError_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	"EGAAdError",
	"EGAAdError",
	Z_Construct_UEnum_GameAnalytics_EGAAdError_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAAdError_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAAdError_Statics::Enum_MetaDataParams), Z_Construct_UEnum_GameAnalytics_EGAAdError_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdError()
{
	if (!Z_Registration_Info_UEnum_EGAAdError.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EGAAdError.InnerSingleton, Z_Construct_UEnum_GameAnalytics_EGAAdError_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EGAAdError.InnerSingleton;
}
// End Enum EGAAdError

// Begin Enum EGAValueType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EGAValueType;
static UEnum* EGAValueType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EGAValueType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EGAValueType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameAnalytics_EGAValueType, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("EGAValueType"));
	}
	return Z_Registration_Info_UEnum_EGAValueType.OuterSingleton;
}
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAValueType>()
{
	return EGAValueType_StaticEnum();
}
struct Z_Construct_UEnum_GameAnalytics_EGAValueType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/GAEnums.h" },
		{ "value_bool.Name", "EGAValueType::value_bool" },
		{ "value_number.Name", "EGAValueType::value_number" },
		{ "value_string.Name", "EGAValueType::value_string" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGAValueType::value_number", (int64)EGAValueType::value_number },
		{ "EGAValueType::value_string", (int64)EGAValueType::value_string },
		{ "EGAValueType::value_bool", (int64)EGAValueType::value_bool },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_GameAnalytics_EGAValueType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	"EGAValueType",
	"EGAValueType",
	Z_Construct_UEnum_GameAnalytics_EGAValueType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAValueType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_GameAnalytics_EGAValueType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_GameAnalytics_EGAValueType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_GameAnalytics_EGAValueType()
{
	if (!Z_Registration_Info_UEnum_EGAValueType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EGAValueType.InnerSingleton, Z_Construct_UEnum_GameAnalytics_EGAValueType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EGAValueType.InnerSingleton;
}
// End Enum EGAValueType

// Begin Registration
struct Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GAEnums_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EGAResourceFlowType_StaticEnum, TEXT("EGAResourceFlowType"), &Z_Registration_Info_UEnum_EGAResourceFlowType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3467320194U) },
		{ EGAProgressionStatus_StaticEnum, TEXT("EGAProgressionStatus"), &Z_Registration_Info_UEnum_EGAProgressionStatus, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 409719171U) },
		{ EGAErrorSeverity_StaticEnum, TEXT("EGAErrorSeverity"), &Z_Registration_Info_UEnum_EGAErrorSeverity, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 772265853U) },
		{ EGAAdAction_StaticEnum, TEXT("EGAAdAction"), &Z_Registration_Info_UEnum_EGAAdAction, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 4156823917U) },
		{ EGAAdType_StaticEnum, TEXT("EGAAdType"), &Z_Registration_Info_UEnum_EGAAdType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2768027247U) },
		{ EGAAdError_StaticEnum, TEXT("EGAAdError"), &Z_Registration_Info_UEnum_EGAAdError, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 4014901624U) },
		{ EGAValueType_StaticEnum, TEXT("EGAValueType"), &Z_Registration_Info_UEnum_EGAValueType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 655476377U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GAEnums_h_4187853921(TEXT("/Script/GameAnalytics"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GAEnums_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GAEnums_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
