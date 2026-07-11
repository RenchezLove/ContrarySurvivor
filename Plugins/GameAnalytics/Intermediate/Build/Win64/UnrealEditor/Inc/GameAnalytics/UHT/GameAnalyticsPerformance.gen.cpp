// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameAnalytics/Private/GameAnalyticsPerformance.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeGameAnalyticsPerformance() {}

// Begin Cross Module References
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
GAMEANALYTICS_API UClass* Z_Construct_UClass_UGameAnalyticsPerformance();
GAMEANALYTICS_API UClass* Z_Construct_UClass_UGameAnalyticsPerformance_NoRegister();
UPackage* Z_Construct_UPackage__Script_GameAnalytics();
// End Cross Module References

// Begin Class UGameAnalyticsPerformance
void UGameAnalyticsPerformance::StaticRegisterNativesUGameAnalyticsPerformance()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UGameAnalyticsPerformance);
UClass* Z_Construct_UClass_UGameAnalyticsPerformance_NoRegister()
{
	return UGameAnalyticsPerformance::StaticClass();
}
struct Z_Construct_UClass_UGameAnalyticsPerformance_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "GameAnalyticsPerformance.h" },
		{ "ModuleRelativePath", "Private/GameAnalyticsPerformance.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGameAnalyticsPerformance>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_UGameAnalyticsPerformance_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_GameAnalytics,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalyticsPerformance_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UGameAnalyticsPerformance_Statics::ClassParams = {
	&UGameAnalyticsPerformance::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x000000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalyticsPerformance_Statics::Class_MetaDataParams), Z_Construct_UClass_UGameAnalyticsPerformance_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UGameAnalyticsPerformance()
{
	if (!Z_Registration_Info_UClass_UGameAnalyticsPerformance.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGameAnalyticsPerformance.OuterSingleton, Z_Construct_UClass_UGameAnalyticsPerformance_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UGameAnalyticsPerformance.OuterSingleton;
}
template<> GAMEANALYTICS_API UClass* StaticClass<UGameAnalyticsPerformance>()
{
	return UGameAnalyticsPerformance::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UGameAnalyticsPerformance);
// End Class UGameAnalyticsPerformance

// Begin Registration
struct Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Private_GameAnalyticsPerformance_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGameAnalyticsPerformance, UGameAnalyticsPerformance::StaticClass, TEXT("UGameAnalyticsPerformance"), &Z_Registration_Info_UClass_UGameAnalyticsPerformance, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGameAnalyticsPerformance), 1110708880U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Private_GameAnalyticsPerformance_h_188480(TEXT("/Script/GameAnalytics"),
	Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Private_GameAnalyticsPerformance_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Private_GameAnalyticsPerformance_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
