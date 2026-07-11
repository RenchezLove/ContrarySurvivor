// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "GameAnalyticsProjectSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef GAMEANALYTICSEDITOR_GameAnalyticsProjectSettings_generated_h
#error "GameAnalyticsProjectSettings.generated.h already included, missing '#pragma once' in GameAnalyticsProjectSettings.h"
#endif
#define GAMEANALYTICSEDITOR_GameAnalyticsProjectSettings_generated_h

#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_9_INCLASS \
private: \
	static void StaticRegisterNativesUGameAnalyticsProjectSettings(); \
	friend struct Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics; \
public: \
	DECLARE_CLASS(UGameAnalyticsProjectSettings, UObject, COMPILED_IN_FLAGS(0 | CLASS_DefaultConfig | CLASS_Config), CASTCLASS_None, TEXT("/Script/GameAnalyticsEditor"), NO_API) \
	DECLARE_SERIALIZER(UGameAnalyticsProjectSettings) \
	static const TCHAR* StaticConfigName() {return TEXT("Engine");} \



#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_9_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGameAnalyticsProjectSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGameAnalyticsProjectSettings) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGameAnalyticsProjectSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGameAnalyticsProjectSettings); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UGameAnalyticsProjectSettings(UGameAnalyticsProjectSettings&&); \
	UGameAnalyticsProjectSettings(const UGameAnalyticsProjectSettings&); \
public: \
	NO_API virtual ~UGameAnalyticsProjectSettings();


#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_6_PROLOG
#define FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_9_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_9_INCLASS \
	FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_9_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> GAMEANALYTICSEDITOR_API UClass* StaticClass<class UGameAnalyticsProjectSettings>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
