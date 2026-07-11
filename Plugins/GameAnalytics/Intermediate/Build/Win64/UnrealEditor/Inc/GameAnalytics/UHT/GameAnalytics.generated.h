// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "GameAnalytics.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class UGameAnalytics;
enum class EGAAdAction : uint8;
enum class EGAAdError : uint8;
enum class EGAAdType : uint8;
enum class EGAErrorSeverity : uint8;
enum class EGAProgressionStatus : uint8;
enum class EGAResourceFlowType : uint8;
struct FGACustomFields;
#ifdef GAMEANALYTICS_GameAnalytics_generated_h
#error "GameAnalytics.generated.h already included, missing '#pragma once' in GameAnalytics.h"
#endif
#define GAMEANALYTICS_GameAnalytics_generated_h

#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_22_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FGACustomValue_Statics; \
	GAMEANALYTICS_API static class UScriptStruct* StaticStruct();


template<> GAMEANALYTICS_API UScriptStruct* StaticStruct<struct FGACustomValue>();

#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_43_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FGACustomFields_Statics; \
	static class UScriptStruct* StaticStruct();


template<> GAMEANALYTICS_API UScriptStruct* StaticStruct<struct FGACustomFields>();

#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_64_RPC_WRAPPERS \
	DECLARE_FUNCTION(execSetWritablePath); \
	DECLARE_FUNCTION(execGetElapsedTimeFromAllSessions); \
	DECLARE_FUNCTION(execGetElapsedSessionTime); \
	DECLARE_FUNCTION(execEnableHealthHardwareInfo); \
	DECLARE_FUNCTION(execEnableMemoryHistogram); \
	DECLARE_FUNCTION(execEnableFpsHistogram); \
	DECLARE_FUNCTION(execEnableSDKInitEvent); \
	DECLARE_FUNCTION(execEnableAdvertisingId); \
	DECLARE_FUNCTION(execOnQuit); \
	DECLARE_FUNCTION(execGetABTestingVariantId); \
	DECLARE_FUNCTION(execGetABTestingId); \
	DECLARE_FUNCTION(execGetRemoteConfigsContentAsString); \
	DECLARE_FUNCTION(execIsRemoteConfigsReady); \
	DECLARE_FUNCTION(execGetRemoteConfigsValueAsString); \
	DECLARE_FUNCTION(execGetExternalUserId); \
	DECLARE_FUNCTION(execGetUserId); \
	DECLARE_FUNCTION(execSetCustomDimension03); \
	DECLARE_FUNCTION(execSetCustomDimension02); \
	DECLARE_FUNCTION(execSetCustomDimension01); \
	DECLARE_FUNCTION(execAddAdEventWithReason); \
	DECLARE_FUNCTION(execAddAdEventWithDuration); \
	DECLARE_FUNCTION(execAddAdEvent); \
	DECLARE_FUNCTION(execAddErrorEvent); \
	DECLARE_FUNCTION(execAddDesignEventWithValue); \
	DECLARE_FUNCTION(execAddDesignEvent); \
	DECLARE_FUNCTION(execAddProgressionEventWithScore); \
	DECLARE_FUNCTION(execAddProgressionEvent); \
	DECLARE_FUNCTION(execAddResourceEvent); \
	DECLARE_FUNCTION(execAddBusinessEventWithReceipt); \
	DECLARE_FUNCTION(execAddBusinessEventAndAutoFetchReceipt); \
	DECLARE_FUNCTION(execAddBusinessEvent); \
	DECLARE_FUNCTION(execEndSession); \
	DECLARE_FUNCTION(execStartSession); \
	DECLARE_FUNCTION(execSetEnabledEventSubmission); \
	DECLARE_FUNCTION(execSetEnabledErrorReporting); \
	DECLARE_FUNCTION(execSetEnabledManualSessionHandling); \
	DECLARE_FUNCTION(execSetEnabledVerboseLog); \
	DECLARE_FUNCTION(execSetEnabledInfoLog); \
	DECLARE_FUNCTION(execInitialize); \
	DECLARE_FUNCTION(execConfigureGameEngineVersion); \
	DECLARE_FUNCTION(execConfigureSdkGameEngineVersion); \
	DECLARE_FUNCTION(execConfigureExternalUserId); \
	DECLARE_FUNCTION(execConfigureUserId); \
	DECLARE_FUNCTION(execDisableDeviceInfo); \
	DECLARE_FUNCTION(execConfigureAutoDetectAppVersion); \
	DECLARE_FUNCTION(execConfigureBuild); \
	DECLARE_FUNCTION(execConfigureAvailableResourceItemTypes); \
	DECLARE_FUNCTION(execConfigureAvailableResourceCurrencies); \
	DECLARE_FUNCTION(execConfigureAvailableCustomDimensions03); \
	DECLARE_FUNCTION(execConfigureAvailableCustomDimensions02); \
	DECLARE_FUNCTION(execConfigureAvailableCustomDimensions01); \
	DECLARE_FUNCTION(execGetInstance);


#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_64_INCLASS \
private: \
	static void StaticRegisterNativesUGameAnalytics(); \
	friend struct Z_Construct_UClass_UGameAnalytics_Statics; \
public: \
	DECLARE_CLASS(UGameAnalytics, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/GameAnalytics"), NO_API) \
	DECLARE_SERIALIZER(UGameAnalytics)


#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_64_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGameAnalytics(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGameAnalytics) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGameAnalytics); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGameAnalytics); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UGameAnalytics(UGameAnalytics&&); \
	UGameAnalytics(const UGameAnalytics&); \
public: \
	NO_API virtual ~UGameAnalytics();


#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_61_PROLOG
#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_64_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_64_RPC_WRAPPERS \
	FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_64_INCLASS \
	FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_64_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> GAMEANALYTICS_API UClass* StaticClass<class UGameAnalytics>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
