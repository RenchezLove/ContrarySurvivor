// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameAnalyticsEditor/Public/GameAnalyticsProjectSettings.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeGameAnalyticsProjectSettings() {}

// Begin Cross Module References
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
GAMEANALYTICSEDITOR_API UClass* Z_Construct_UClass_UGameAnalyticsProjectSettings();
GAMEANALYTICSEDITOR_API UClass* Z_Construct_UClass_UGameAnalyticsProjectSettings_NoRegister();
UPackage* Z_Construct_UPackage__Script_GameAnalyticsEditor();
// End Cross Module References

// Begin Class UGameAnalyticsProjectSettings
void UGameAnalyticsProjectSettings::StaticRegisterNativesUGameAnalyticsProjectSettings()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UGameAnalyticsProjectSettings);
UClass* Z_Construct_UClass_UGameAnalyticsProjectSettings_NoRegister()
{
	return UGameAnalyticsProjectSettings::StaticClass();
}
struct Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "GameAnalyticsProjectSettings.h" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_IosGameKey_MetaData[] = {
		{ "Category", "IosSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics iOS Game Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_IosSecretKey_MetaData[] = {
		{ "Category", "IosSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics iOS Secret Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_IosBuild_MetaData[] = {
		{ "Category", "IosSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The current version of the iOS game. Updating the build name for each test version of the game will allow you to filter by build when viewing your data on the GA website." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AndroidGameKey_MetaData[] = {
		{ "Category", "AndroidSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Android Game Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AndroidSecretKey_MetaData[] = {
		{ "Category", "AndroidSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Android Secret Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AndroidBuild_MetaData[] = {
		{ "Category", "AndroidSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The current version of the game. Updating the build name for each test version of the game will allow you to filter by build when viewing your data on the GA website." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MacGameKey_MetaData[] = {
		{ "Category", "MacSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Mac Game Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MacSecretKey_MetaData[] = {
		{ "Category", "MacSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Mac Secret Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MacBuild_MetaData[] = {
		{ "Category", "MacSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The current version of the Mac game. Updating the build name for each test version of the game will allow you to filter by build when viewing your data on the GA website." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_WindowsGameKey_MetaData[] = {
		{ "Category", "WindowsSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Windows Game Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_WindowsSecretKey_MetaData[] = {
		{ "Category", "WindowsSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Windows Secret Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_WindowsBuild_MetaData[] = {
		{ "Category", "WindowsSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The current version of the Windows game. Updating the build name for each test version of the game will allow you to filter by build when viewing your data on the GA website." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LinuxGameKey_MetaData[] = {
		{ "Category", "LinuxSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Linux Game Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LinuxSecretKey_MetaData[] = {
		{ "Category", "LinuxSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics Linux Secret Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LinuxBuild_MetaData[] = {
		{ "Category", "LinuxSetup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The current version of the Linux game. Updating the build name for each test version of the game will allow you to filter by build when viewing your data on the GA website." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Html5GameKey_MetaData[] = {
		{ "Category", "Html5Setup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics HTML5 Game Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Html5SecretKey_MetaData[] = {
		{ "Category", "Html5Setup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Your GameAnalytics HTML5 Secret Key - copy/paste from the GA website or log in to autofill it." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Html5Build_MetaData[] = {
		{ "Category", "Html5Setup" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The current version of the HTML5 game. Updating the build name for each test version of the game will allow you to filter by build when viewing your data on the GA website." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomDimensions01_MetaData[] = {
		{ "Category", "CustomDimensions" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "List of custom dimensions 01." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomDimensions02_MetaData[] = {
		{ "Category", "CustomDimensions" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "List of custom dimensions 02." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomDimensions03_MetaData[] = {
		{ "Category", "CustomDimensions" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "List of custom dimensions 03." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ResourceCurrencies_MetaData[] = {
		{ "Category", "ResourceTypes" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "List of Resource Currencies." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ResourceItemTypes_MetaData[] = {
		{ "Category", "ResourceTypes" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "List of Resource Item Types." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UseManualSessionHandling_MetaData[] = {
		{ "Category", "Advanced" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Use manual session handling. Manually choose when to end and start a new session. Note initializing of the SDK will automatically start the first session." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AutoDetectAppVersion_MetaData[] = {
		{ "Category", "Advanced" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Auto detect app version to use for build field (only for Android and iOS)." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DisableDeviceInfo_MetaData[] = {
		{ "Category", "Advanced" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Disable device info (only Mac, Windows and Linux)." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UseErrorReporting_MetaData[] = {
		{ "Category", "Advanced" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Use automatic error reporting." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_InfoLogBuild_MetaData[] = {
		{ "Category", "Debug" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Show info messages from GA in builds (f.x. Xcode for iOS)." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_VerboseLogBuild_MetaData[] = {
		{ "Category", "Debug" },
		{ "ModuleRelativePath", "Public/GameAnalyticsProjectSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Show full info messages from GA in builds (f.x. Xcode for iOS). Noet that this option includes long JSON messages sent to the server." },
#endif
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_IosGameKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_IosSecretKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_IosBuild;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AndroidGameKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AndroidSecretKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AndroidBuild;
	static const UECodeGen_Private::FStrPropertyParams NewProp_MacGameKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_MacSecretKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_MacBuild;
	static const UECodeGen_Private::FStrPropertyParams NewProp_WindowsGameKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_WindowsSecretKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_WindowsBuild;
	static const UECodeGen_Private::FStrPropertyParams NewProp_LinuxGameKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_LinuxSecretKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_LinuxBuild;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Html5GameKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Html5SecretKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Html5Build;
	static const UECodeGen_Private::FStrPropertyParams NewProp_CustomDimensions01_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_CustomDimensions01;
	static const UECodeGen_Private::FStrPropertyParams NewProp_CustomDimensions02_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_CustomDimensions02;
	static const UECodeGen_Private::FStrPropertyParams NewProp_CustomDimensions03_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_CustomDimensions03;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ResourceCurrencies_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_ResourceCurrencies;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ResourceItemTypes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_ResourceItemTypes;
	static void NewProp_UseManualSessionHandling_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_UseManualSessionHandling;
	static void NewProp_AutoDetectAppVersion_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_AutoDetectAppVersion;
	static void NewProp_DisableDeviceInfo_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DisableDeviceInfo;
	static void NewProp_UseErrorReporting_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_UseErrorReporting;
	static void NewProp_InfoLogBuild_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_InfoLogBuild;
	static void NewProp_VerboseLogBuild_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_VerboseLogBuild;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGameAnalyticsProjectSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_IosGameKey = { "IosGameKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, IosGameKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_IosGameKey_MetaData), NewProp_IosGameKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_IosSecretKey = { "IosSecretKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, IosSecretKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_IosSecretKey_MetaData), NewProp_IosSecretKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_IosBuild = { "IosBuild", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, IosBuild), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_IosBuild_MetaData), NewProp_IosBuild_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AndroidGameKey = { "AndroidGameKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, AndroidGameKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AndroidGameKey_MetaData), NewProp_AndroidGameKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AndroidSecretKey = { "AndroidSecretKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, AndroidSecretKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AndroidSecretKey_MetaData), NewProp_AndroidSecretKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AndroidBuild = { "AndroidBuild", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, AndroidBuild), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AndroidBuild_MetaData), NewProp_AndroidBuild_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_MacGameKey = { "MacGameKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, MacGameKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MacGameKey_MetaData), NewProp_MacGameKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_MacSecretKey = { "MacSecretKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, MacSecretKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MacSecretKey_MetaData), NewProp_MacSecretKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_MacBuild = { "MacBuild", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, MacBuild), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MacBuild_MetaData), NewProp_MacBuild_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_WindowsGameKey = { "WindowsGameKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, WindowsGameKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_WindowsGameKey_MetaData), NewProp_WindowsGameKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_WindowsSecretKey = { "WindowsSecretKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, WindowsSecretKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_WindowsSecretKey_MetaData), NewProp_WindowsSecretKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_WindowsBuild = { "WindowsBuild", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, WindowsBuild), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_WindowsBuild_MetaData), NewProp_WindowsBuild_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_LinuxGameKey = { "LinuxGameKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, LinuxGameKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LinuxGameKey_MetaData), NewProp_LinuxGameKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_LinuxSecretKey = { "LinuxSecretKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, LinuxSecretKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LinuxSecretKey_MetaData), NewProp_LinuxSecretKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_LinuxBuild = { "LinuxBuild", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, LinuxBuild), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LinuxBuild_MetaData), NewProp_LinuxBuild_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_Html5GameKey = { "Html5GameKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, Html5GameKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Html5GameKey_MetaData), NewProp_Html5GameKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_Html5SecretKey = { "Html5SecretKey", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, Html5SecretKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Html5SecretKey_MetaData), NewProp_Html5SecretKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_Html5Build = { "Html5Build", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, Html5Build), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Html5Build_MetaData), NewProp_Html5Build_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions01_Inner = { "CustomDimensions01", nullptr, (EPropertyFlags)0x0000000000004000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions01 = { "CustomDimensions01", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, CustomDimensions01), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomDimensions01_MetaData), NewProp_CustomDimensions01_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions02_Inner = { "CustomDimensions02", nullptr, (EPropertyFlags)0x0000000000004000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions02 = { "CustomDimensions02", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, CustomDimensions02), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomDimensions02_MetaData), NewProp_CustomDimensions02_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions03_Inner = { "CustomDimensions03", nullptr, (EPropertyFlags)0x0000000000004000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions03 = { "CustomDimensions03", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, CustomDimensions03), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomDimensions03_MetaData), NewProp_CustomDimensions03_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceCurrencies_Inner = { "ResourceCurrencies", nullptr, (EPropertyFlags)0x0000000000004000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceCurrencies = { "ResourceCurrencies", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, ResourceCurrencies), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ResourceCurrencies_MetaData), NewProp_ResourceCurrencies_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceItemTypes_Inner = { "ResourceItemTypes", nullptr, (EPropertyFlags)0x0000000000004000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceItemTypes = { "ResourceItemTypes", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UGameAnalyticsProjectSettings, ResourceItemTypes), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ResourceItemTypes_MetaData), NewProp_ResourceItemTypes_MetaData) };
void Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseManualSessionHandling_SetBit(void* Obj)
{
	((UGameAnalyticsProjectSettings*)Obj)->UseManualSessionHandling = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseManualSessionHandling = { "UseManualSessionHandling", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UGameAnalyticsProjectSettings), &Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseManualSessionHandling_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UseManualSessionHandling_MetaData), NewProp_UseManualSessionHandling_MetaData) };
void Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AutoDetectAppVersion_SetBit(void* Obj)
{
	((UGameAnalyticsProjectSettings*)Obj)->AutoDetectAppVersion = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AutoDetectAppVersion = { "AutoDetectAppVersion", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UGameAnalyticsProjectSettings), &Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AutoDetectAppVersion_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AutoDetectAppVersion_MetaData), NewProp_AutoDetectAppVersion_MetaData) };
void Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_DisableDeviceInfo_SetBit(void* Obj)
{
	((UGameAnalyticsProjectSettings*)Obj)->DisableDeviceInfo = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_DisableDeviceInfo = { "DisableDeviceInfo", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UGameAnalyticsProjectSettings), &Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_DisableDeviceInfo_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DisableDeviceInfo_MetaData), NewProp_DisableDeviceInfo_MetaData) };
void Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseErrorReporting_SetBit(void* Obj)
{
	((UGameAnalyticsProjectSettings*)Obj)->UseErrorReporting = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseErrorReporting = { "UseErrorReporting", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UGameAnalyticsProjectSettings), &Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseErrorReporting_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UseErrorReporting_MetaData), NewProp_UseErrorReporting_MetaData) };
void Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_InfoLogBuild_SetBit(void* Obj)
{
	((UGameAnalyticsProjectSettings*)Obj)->InfoLogBuild = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_InfoLogBuild = { "InfoLogBuild", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UGameAnalyticsProjectSettings), &Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_InfoLogBuild_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_InfoLogBuild_MetaData), NewProp_InfoLogBuild_MetaData) };
void Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_VerboseLogBuild_SetBit(void* Obj)
{
	((UGameAnalyticsProjectSettings*)Obj)->VerboseLogBuild = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_VerboseLogBuild = { "VerboseLogBuild", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UGameAnalyticsProjectSettings), &Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_VerboseLogBuild_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_VerboseLogBuild_MetaData), NewProp_VerboseLogBuild_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_IosGameKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_IosSecretKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_IosBuild,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AndroidGameKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AndroidSecretKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AndroidBuild,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_MacGameKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_MacSecretKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_MacBuild,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_WindowsGameKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_WindowsSecretKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_WindowsBuild,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_LinuxGameKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_LinuxSecretKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_LinuxBuild,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_Html5GameKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_Html5SecretKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_Html5Build,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions01_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions01,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions02_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions02,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions03_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_CustomDimensions03,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceCurrencies_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceCurrencies,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceItemTypes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_ResourceItemTypes,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseManualSessionHandling,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_AutoDetectAppVersion,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_DisableDeviceInfo,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_UseErrorReporting,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_InfoLogBuild,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::NewProp_VerboseLogBuild,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_GameAnalyticsEditor,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::ClassParams = {
	&UGameAnalyticsProjectSettings::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::PropPointers),
	0,
	0x000000A6u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UGameAnalyticsProjectSettings()
{
	if (!Z_Registration_Info_UClass_UGameAnalyticsProjectSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGameAnalyticsProjectSettings.OuterSingleton, Z_Construct_UClass_UGameAnalyticsProjectSettings_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UGameAnalyticsProjectSettings.OuterSingleton;
}
template<> GAMEANALYTICSEDITOR_API UClass* StaticClass<UGameAnalyticsProjectSettings>()
{
	return UGameAnalyticsProjectSettings::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UGameAnalyticsProjectSettings);
UGameAnalyticsProjectSettings::~UGameAnalyticsProjectSettings() {}
// End Class UGameAnalyticsProjectSettings

// Begin Registration
struct Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGameAnalyticsProjectSettings, UGameAnalyticsProjectSettings::StaticClass, TEXT("UGameAnalyticsProjectSettings"), &Z_Registration_Info_UClass_UGameAnalyticsProjectSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGameAnalyticsProjectSettings), 979237879U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_3290624452(TEXT("/Script/GameAnalyticsEditor"),
	Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalyticsEditor_Public_GameAnalyticsProjectSettings_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
