// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "GAEnums.h"
#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef GAMEANALYTICS_GAEnums_generated_h
#error "GAEnums.generated.h already included, missing '#pragma once' in GAEnums.h"
#endif
#define GAMEANALYTICS_GAEnums_generated_h

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GAEnums_h


#define FOREACH_ENUM_EGARESOURCEFLOWTYPE(op) \
	op(EGAResourceFlowType::undefined) \
	op(EGAResourceFlowType::source) \
	op(EGAResourceFlowType::sink) 

enum class EGAResourceFlowType : uint8;
template<> struct TIsUEnumClass<EGAResourceFlowType> { enum { Value = true }; };
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAResourceFlowType>();

#define FOREACH_ENUM_EGAPROGRESSIONSTATUS(op) \
	op(EGAProgressionStatus::undefined) \
	op(EGAProgressionStatus::start) \
	op(EGAProgressionStatus::complete) \
	op(EGAProgressionStatus::fail) 

enum class EGAProgressionStatus : uint8;
template<> struct TIsUEnumClass<EGAProgressionStatus> { enum { Value = true }; };
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAProgressionStatus>();

#define FOREACH_ENUM_EGAERRORSEVERITY(op) \
	op(EGAErrorSeverity::undefined) \
	op(EGAErrorSeverity::debug) \
	op(EGAErrorSeverity::info) \
	op(EGAErrorSeverity::warning) \
	op(EGAErrorSeverity::error) \
	op(EGAErrorSeverity::critical) 

enum class EGAErrorSeverity : uint8;
template<> struct TIsUEnumClass<EGAErrorSeverity> { enum { Value = true }; };
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAErrorSeverity>();

#define FOREACH_ENUM_EGAADACTION(op) \
	op(EGAAdAction::undefined) \
	op(EGAAdAction::clicked) \
	op(EGAAdAction::show) \
	op(EGAAdAction::failedshow) \
	op(EGAAdAction::rewardreceived) \
	op(EGAAdAction::request) \
	op(EGAAdAction::loaded) 

enum class EGAAdAction : uint8;
template<> struct TIsUEnumClass<EGAAdAction> { enum { Value = true }; };
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAAdAction>();

#define FOREACH_ENUM_EGAADTYPE(op) \
	op(EGAAdType::undefined) \
	op(EGAAdType::video) \
	op(EGAAdType::rewardedvideo) \
	op(EGAAdType::playable) \
	op(EGAAdType::interstitial) \
	op(EGAAdType::offerwall) \
	op(EGAAdType::banner) 

enum class EGAAdType : uint8;
template<> struct TIsUEnumClass<EGAAdType> { enum { Value = true }; };
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAAdType>();

#define FOREACH_ENUM_EGAADERROR(op) \
	op(EGAAdError::undefined) \
	op(EGAAdError::unknown) \
	op(EGAAdError::offline) \
	op(EGAAdError::nofill) \
	op(EGAAdError::internalerror) \
	op(EGAAdError::invalidrequest) \
	op(EGAAdError::unabletoprecache) 

enum class EGAAdError : uint8;
template<> struct TIsUEnumClass<EGAAdError> { enum { Value = true }; };
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAAdError>();

#define FOREACH_ENUM_EGAVALUETYPE(op) \
	op(EGAValueType::value_number) \
	op(EGAValueType::value_string) \
	op(EGAValueType::value_bool) 

enum class EGAValueType : uint8;
template<> struct TIsUEnumClass<EGAValueType> { enum { Value = true }; };
template<> GAMEANALYTICS_API UEnum* StaticEnum<EGAValueType>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
