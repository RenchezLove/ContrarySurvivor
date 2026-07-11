// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeGameAnalytics_init() {}
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_GameAnalytics;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_GameAnalytics()
	{
		if (!Z_Registration_Info_UPackage__Script_GameAnalytics.OuterSingleton)
		{
			static const UECodeGen_Private::FPackageParams PackageParams = {
				"/Script/GameAnalytics",
				nullptr,
				0,
				PKG_CompiledIn | 0x00000000,
				0x952BCD05,
				0xABDD2018,
				METADATA_PARAMS(0, nullptr)
			};
			UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_GameAnalytics.OuterSingleton, PackageParams);
		}
		return Z_Registration_Info_UPackage__Script_GameAnalytics.OuterSingleton;
	}
	static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_GameAnalytics(Z_Construct_UPackage__Script_GameAnalytics, TEXT("/Script/GameAnalytics"), Z_Registration_Info_UPackage__Script_GameAnalytics, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0x952BCD05, 0xABDD2018));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
