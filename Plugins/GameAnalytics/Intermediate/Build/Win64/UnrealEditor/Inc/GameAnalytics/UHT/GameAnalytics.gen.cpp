// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameAnalytics/Public/GameAnalytics.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeGameAnalytics() {}

// Begin Cross Module References
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
GAMEANALYTICS_API UClass* Z_Construct_UClass_UGameAnalytics();
GAMEANALYTICS_API UClass* Z_Construct_UClass_UGameAnalytics_NoRegister();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdAction();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdError();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAAdType();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType();
GAMEANALYTICS_API UEnum* Z_Construct_UEnum_GameAnalytics_EGAValueType();
GAMEANALYTICS_API UScriptStruct* Z_Construct_UScriptStruct_FGACustomFields();
GAMEANALYTICS_API UScriptStruct* Z_Construct_UScriptStruct_FGACustomValue();
UPackage* Z_Construct_UPackage__Script_GameAnalytics();
// End Cross Module References

// Begin ScriptStruct FGACustomValue
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_GACustomValue;
class UScriptStruct* FGACustomValue::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_GACustomValue.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_GACustomValue.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGACustomValue, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("GACustomValue"));
	}
	return Z_Registration_Info_UScriptStruct_GACustomValue.OuterSingleton;
}
template<> GAMEANALYTICS_API UScriptStruct* StaticStruct<FGACustomValue>()
{
	return FGACustomValue::StaticStruct();
}
struct Z_Construct_UScriptStruct_FGACustomValue_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Key_MetaData[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ValueString_MetaData[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ValueNumber_MetaData[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ValueBool_MetaData[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ValueType_MetaData[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Key;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ValueString;
	static const UECodeGen_Private::FDoublePropertyParams NewProp_ValueNumber;
	static void NewProp_ValueBool_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ValueBool;
	static const UECodeGen_Private::FBytePropertyParams NewProp_ValueType_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ValueType;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGACustomValue>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_Key = { "Key", nullptr, (EPropertyFlags)0x0010000000000004, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FGACustomValue, Key), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Key_MetaData), NewProp_Key_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueString = { "ValueString", nullptr, (EPropertyFlags)0x0010000000000004, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FGACustomValue, ValueString), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ValueString_MetaData), NewProp_ValueString_MetaData) };
const UECodeGen_Private::FDoublePropertyParams Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueNumber = { "ValueNumber", nullptr, (EPropertyFlags)0x0010000000000004, UECodeGen_Private::EPropertyGenFlags::Double, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FGACustomValue, ValueNumber), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ValueNumber_MetaData), NewProp_ValueNumber_MetaData) };
void Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueBool_SetBit(void* Obj)
{
	((FGACustomValue*)Obj)->ValueBool = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueBool = { "ValueBool", nullptr, (EPropertyFlags)0x0010000000000004, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FGACustomValue), &Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueBool_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ValueBool_MetaData), NewProp_ValueBool_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueType = { "ValueType", nullptr, (EPropertyFlags)0x0010000000000004, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FGACustomValue, ValueType), Z_Construct_UEnum_GameAnalytics_EGAValueType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ValueType_MetaData), NewProp_ValueType_MetaData) }; // 655476377
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FGACustomValue_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_Key,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueString,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueNumber,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueBool,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueType_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomValue_Statics::NewProp_ValueType,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FGACustomValue_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FGACustomValue_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	&NewStructOps,
	"GACustomValue",
	Z_Construct_UScriptStruct_FGACustomValue_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FGACustomValue_Statics::PropPointers),
	sizeof(FGACustomValue),
	alignof(FGACustomValue),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000001),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FGACustomValue_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FGACustomValue_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FGACustomValue()
{
	if (!Z_Registration_Info_UScriptStruct_GACustomValue.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_GACustomValue.InnerSingleton, Z_Construct_UScriptStruct_FGACustomValue_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_GACustomValue.InnerSingleton;
}
// End ScriptStruct FGACustomValue

// Begin ScriptStruct FGACustomFields
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_GACustomFields;
class UScriptStruct* FGACustomFields::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_GACustomFields.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_GACustomFields.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGACustomFields, (UObject*)Z_Construct_UPackage__Script_GameAnalytics(), TEXT("GACustomFields"));
	}
	return Z_Registration_Info_UScriptStruct_GACustomFields.OuterSingleton;
}
template<> GAMEANALYTICS_API UScriptStruct* StaticStruct<FGACustomFields>()
{
	return FGACustomFields::StaticStruct();
}
struct Z_Construct_UScriptStruct_FGACustomFields_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Values_MetaData[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_Values_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_Values;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGACustomFields>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FGACustomFields_Statics::NewProp_Values_Inner = { "Values", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FGACustomValue, METADATA_PARAMS(0, nullptr) }; // 3328573524
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FGACustomFields_Statics::NewProp_Values = { "Values", nullptr, (EPropertyFlags)0x0010000000000004, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FGACustomFields, Values), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Values_MetaData), NewProp_Values_MetaData) }; // 3328573524
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FGACustomFields_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomFields_Statics::NewProp_Values_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FGACustomFields_Statics::NewProp_Values,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FGACustomFields_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FGACustomFields_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_GameAnalytics,
	nullptr,
	&NewStructOps,
	"GACustomFields",
	Z_Construct_UScriptStruct_FGACustomFields_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FGACustomFields_Statics::PropPointers),
	sizeof(FGACustomFields),
	alignof(FGACustomFields),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FGACustomFields_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FGACustomFields_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FGACustomFields()
{
	if (!Z_Registration_Info_UScriptStruct_GACustomFields.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_GACustomFields.InnerSingleton, Z_Construct_UScriptStruct_FGACustomFields_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_GACustomFields.InnerSingleton;
}
// End ScriptStruct FGACustomFields

// Begin Class UGameAnalytics Function AddAdEvent
struct Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics
{
	struct GameAnalytics_eventAddAdEvent_Parms
	{
		EGAAdAction Action;
		EGAAdType AdType;
		FString AdSdkName;
		FString AdPlacement;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "//////////////////////////////////////////////////////\n// AD EVENTS ARE ONLY AVAILABLE FOR IOS AND ANDROID //\n//////////////////////////////////////////////////////\n" },
#endif
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "AD EVENTS ARE ONLY AVAILABLE FOR IOS AND ANDROID" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AdSdkName_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AdPlacement_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_Action_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Action;
	static const UECodeGen_Private::FBytePropertyParams NewProp_AdType_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_AdType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AdSdkName;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AdPlacement;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_Action_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_Action = { "Action", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEvent_Parms, Action), Z_Construct_UEnum_GameAnalytics_EGAAdAction, METADATA_PARAMS(0, nullptr) }; // 4156823917
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdType = { "AdType", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEvent_Parms, AdType), Z_Construct_UEnum_GameAnalytics_EGAAdType, METADATA_PARAMS(0, nullptr) }; // 2768027247
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdSdkName = { "AdSdkName", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEvent_Parms, AdSdkName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AdSdkName_MetaData), NewProp_AdSdkName_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdPlacement = { "AdPlacement", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEvent_Parms, AdPlacement), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AdPlacement_MetaData), NewProp_AdPlacement_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEvent_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddAdEvent_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddAdEvent_Parms), &Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_Action_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_Action,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdType_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdSdkName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_AdPlacement,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddAdEvent", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::GameAnalytics_eventAddAdEvent_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::GameAnalytics_eventAddAdEvent_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddAdEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddAdEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddAdEvent)
{
	P_GET_ENUM(EGAAdAction,Z_Param_Action);
	P_GET_ENUM(EGAAdType,Z_Param_AdType);
	P_GET_PROPERTY(FStrProperty,Z_Param_AdSdkName);
	P_GET_PROPERTY(FStrProperty,Z_Param_AdPlacement);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddAdEvent(EGAAdAction(Z_Param_Action),EGAAdType(Z_Param_AdType),Z_Param_AdSdkName,Z_Param_AdPlacement,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddAdEvent

// Begin Class UGameAnalytics Function AddAdEventWithDuration
struct Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics
{
	struct GameAnalytics_eventAddAdEventWithDuration_Parms
	{
		EGAAdAction Action;
		EGAAdType AdType;
		FString AdSdkName;
		FString AdPlacement;
		int64 Duration;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AdSdkName_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AdPlacement_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_Action_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Action;
	static const UECodeGen_Private::FBytePropertyParams NewProp_AdType_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_AdType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AdSdkName;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AdPlacement;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_Duration;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_Action_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_Action = { "Action", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithDuration_Parms, Action), Z_Construct_UEnum_GameAnalytics_EGAAdAction, METADATA_PARAMS(0, nullptr) }; // 4156823917
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdType = { "AdType", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithDuration_Parms, AdType), Z_Construct_UEnum_GameAnalytics_EGAAdType, METADATA_PARAMS(0, nullptr) }; // 2768027247
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdSdkName = { "AdSdkName", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithDuration_Parms, AdSdkName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AdSdkName_MetaData), NewProp_AdSdkName_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdPlacement = { "AdPlacement", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithDuration_Parms, AdPlacement), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AdPlacement_MetaData), NewProp_AdPlacement_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_Duration = { "Duration", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithDuration_Parms, Duration), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithDuration_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddAdEventWithDuration_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddAdEventWithDuration_Parms), &Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_Action_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_Action,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdType_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdSdkName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_AdPlacement,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_Duration,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddAdEventWithDuration", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::GameAnalytics_eventAddAdEventWithDuration_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::GameAnalytics_eventAddAdEventWithDuration_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddAdEventWithDuration)
{
	P_GET_ENUM(EGAAdAction,Z_Param_Action);
	P_GET_ENUM(EGAAdType,Z_Param_AdType);
	P_GET_PROPERTY(FStrProperty,Z_Param_AdSdkName);
	P_GET_PROPERTY(FStrProperty,Z_Param_AdPlacement);
	P_GET_PROPERTY(FInt64Property,Z_Param_Duration);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddAdEventWithDuration(EGAAdAction(Z_Param_Action),EGAAdType(Z_Param_AdType),Z_Param_AdSdkName,Z_Param_AdPlacement,Z_Param_Duration,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddAdEventWithDuration

// Begin Class UGameAnalytics Function AddAdEventWithReason
struct Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics
{
	struct GameAnalytics_eventAddAdEventWithReason_Parms
	{
		EGAAdAction Action;
		EGAAdType AdType;
		FString AdSdkName;
		FString AdPlacement;
		EGAAdError Reason;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AdSdkName_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AdPlacement_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_Action_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Action;
	static const UECodeGen_Private::FBytePropertyParams NewProp_AdType_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_AdType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AdSdkName;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AdPlacement;
	static const UECodeGen_Private::FBytePropertyParams NewProp_Reason_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Reason;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Action_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Action = { "Action", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithReason_Parms, Action), Z_Construct_UEnum_GameAnalytics_EGAAdAction, METADATA_PARAMS(0, nullptr) }; // 4156823917
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdType = { "AdType", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithReason_Parms, AdType), Z_Construct_UEnum_GameAnalytics_EGAAdType, METADATA_PARAMS(0, nullptr) }; // 2768027247
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdSdkName = { "AdSdkName", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithReason_Parms, AdSdkName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AdSdkName_MetaData), NewProp_AdSdkName_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdPlacement = { "AdPlacement", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithReason_Parms, AdPlacement), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AdPlacement_MetaData), NewProp_AdPlacement_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Reason_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Reason = { "Reason", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithReason_Parms, Reason), Z_Construct_UEnum_GameAnalytics_EGAAdError, METADATA_PARAMS(0, nullptr) }; // 4014901624
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddAdEventWithReason_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddAdEventWithReason_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddAdEventWithReason_Parms), &Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Action_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Action,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdType_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdSdkName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_AdPlacement,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Reason_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_Reason,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddAdEventWithReason", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::GameAnalytics_eventAddAdEventWithReason_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::GameAnalytics_eventAddAdEventWithReason_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddAdEventWithReason)
{
	P_GET_ENUM(EGAAdAction,Z_Param_Action);
	P_GET_ENUM(EGAAdType,Z_Param_AdType);
	P_GET_PROPERTY(FStrProperty,Z_Param_AdSdkName);
	P_GET_PROPERTY(FStrProperty,Z_Param_AdPlacement);
	P_GET_ENUM(EGAAdError,Z_Param_Reason);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddAdEventWithReason(EGAAdAction(Z_Param_Action),EGAAdType(Z_Param_AdType),Z_Param_AdSdkName,Z_Param_AdPlacement,EGAAdError(Z_Param_Reason),Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddAdEventWithReason

// Begin Class UGameAnalytics Function AddBusinessEvent
struct Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics
{
	struct GameAnalytics_eventAddBusinessEvent_Parms
	{
		FString Currency;
		int32 Amount;
		FString ItemType;
		FString ItemId;
		FString CartType;
		FString Receipt;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "//////////////////////////////////////////////////////\n" },
#endif
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "CPP_Default_Receipt", "" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Currency_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemType_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemId_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CartType_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Receipt_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Currency;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Amount;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemId;
	static const UECodeGen_Private::FStrPropertyParams NewProp_CartType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Receipt;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_Currency = { "Currency", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEvent_Parms, Currency), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Currency_MetaData), NewProp_Currency_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_Amount = { "Amount", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEvent_Parms, Amount), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_ItemType = { "ItemType", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEvent_Parms, ItemType), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemType_MetaData), NewProp_ItemType_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_ItemId = { "ItemId", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEvent_Parms, ItemId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemId_MetaData), NewProp_ItemId_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_CartType = { "CartType", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEvent_Parms, CartType), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CartType_MetaData), NewProp_CartType_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_Receipt = { "Receipt", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEvent_Parms, Receipt), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Receipt_MetaData), NewProp_Receipt_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEvent_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddBusinessEvent_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddBusinessEvent_Parms), &Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_Currency,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_Amount,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_ItemType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_ItemId,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_CartType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_Receipt,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddBusinessEvent", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::GameAnalytics_eventAddBusinessEvent_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::GameAnalytics_eventAddBusinessEvent_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddBusinessEvent)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Currency);
	P_GET_PROPERTY(FIntProperty,Z_Param_Amount);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_ItemType);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_ItemId);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_CartType);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Receipt);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddBusinessEvent(Z_Param_Out_Currency,Z_Param_Amount,Z_Param_Out_ItemType,Z_Param_Out_ItemId,Z_Param_Out_CartType,Z_Param_Out_Receipt,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddBusinessEvent

// Begin Class UGameAnalytics Function AddBusinessEventAndAutoFetchReceipt
struct Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics
{
	struct GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms
	{
		FString Currency;
		int32 Amount;
		FString ItemType;
		FString ItemId;
		FString CartType;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Currency_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemType_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemId_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CartType_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Currency;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Amount;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemId;
	static const UECodeGen_Private::FStrPropertyParams NewProp_CartType;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_Currency = { "Currency", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms, Currency), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Currency_MetaData), NewProp_Currency_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_Amount = { "Amount", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms, Amount), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_ItemType = { "ItemType", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms, ItemType), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemType_MetaData), NewProp_ItemType_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_ItemId = { "ItemId", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms, ItemId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemId_MetaData), NewProp_ItemId_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_CartType = { "CartType", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms, CartType), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CartType_MetaData), NewProp_CartType_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms), &Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_Currency,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_Amount,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_ItemType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_ItemId,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_CartType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddBusinessEventAndAutoFetchReceipt", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::GameAnalytics_eventAddBusinessEventAndAutoFetchReceipt_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddBusinessEventAndAutoFetchReceipt)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Currency);
	P_GET_PROPERTY(FIntProperty,Z_Param_Amount);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_ItemType);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_ItemId);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_CartType);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddBusinessEventAndAutoFetchReceipt(Z_Param_Out_Currency,Z_Param_Amount,Z_Param_Out_ItemType,Z_Param_Out_ItemId,Z_Param_Out_CartType,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddBusinessEventAndAutoFetchReceipt

// Begin Class UGameAnalytics Function AddBusinessEventWithReceipt
struct Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics
{
	struct GameAnalytics_eventAddBusinessEventWithReceipt_Parms
	{
		FString Currency;
		int32 Amount;
		FString ItemType;
		FString ItemId;
		FString CartType;
		FString Receipt;
		FString Store;
		FString Signature;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Currency_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemType_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemId_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CartType_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Receipt_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Store_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Signature_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Currency;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Amount;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemId;
	static const UECodeGen_Private::FStrPropertyParams NewProp_CartType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Receipt;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Store;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Signature;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Currency = { "Currency", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, Currency), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Currency_MetaData), NewProp_Currency_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Amount = { "Amount", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, Amount), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_ItemType = { "ItemType", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, ItemType), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemType_MetaData), NewProp_ItemType_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_ItemId = { "ItemId", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, ItemId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemId_MetaData), NewProp_ItemId_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_CartType = { "CartType", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, CartType), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CartType_MetaData), NewProp_CartType_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Receipt = { "Receipt", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, Receipt), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Receipt_MetaData), NewProp_Receipt_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Store = { "Store", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, Store), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Store_MetaData), NewProp_Store_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Signature = { "Signature", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, Signature), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Signature_MetaData), NewProp_Signature_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddBusinessEventWithReceipt_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddBusinessEventWithReceipt_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddBusinessEventWithReceipt_Parms), &Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Currency,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Amount,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_ItemType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_ItemId,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_CartType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Receipt,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Store,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_Signature,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddBusinessEventWithReceipt", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::GameAnalytics_eventAddBusinessEventWithReceipt_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::GameAnalytics_eventAddBusinessEventWithReceipt_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddBusinessEventWithReceipt)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Currency);
	P_GET_PROPERTY(FIntProperty,Z_Param_Amount);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_ItemType);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_ItemId);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_CartType);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Receipt);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Store);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Signature);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddBusinessEventWithReceipt(Z_Param_Out_Currency,Z_Param_Amount,Z_Param_Out_ItemType,Z_Param_Out_ItemId,Z_Param_Out_CartType,Z_Param_Out_Receipt,Z_Param_Out_Store,Z_Param_Out_Signature,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddBusinessEventWithReceipt

// Begin Class UGameAnalytics Function AddDesignEvent
struct Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics
{
	struct GameAnalytics_eventAddDesignEvent_Parms
	{
		FString EventId;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_EventId_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_EventId;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_EventId = { "EventId", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddDesignEvent_Parms, EventId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_EventId_MetaData), NewProp_EventId_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddDesignEvent_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddDesignEvent_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddDesignEvent_Parms), &Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_EventId,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddDesignEvent", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::GameAnalytics_eventAddDesignEvent_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::GameAnalytics_eventAddDesignEvent_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddDesignEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddDesignEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddDesignEvent)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_EventId);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddDesignEvent(Z_Param_EventId,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddDesignEvent

// Begin Class UGameAnalytics Function AddDesignEventWithValue
struct Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics
{
	struct GameAnalytics_eventAddDesignEventWithValue_Parms
	{
		FString EventId;
		float Value;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_EventId_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_EventId;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Value;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_EventId = { "EventId", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddDesignEventWithValue_Parms, EventId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_EventId_MetaData), NewProp_EventId_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddDesignEventWithValue_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddDesignEventWithValue_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddDesignEventWithValue_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddDesignEventWithValue_Parms), &Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_EventId,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddDesignEventWithValue", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::GameAnalytics_eventAddDesignEventWithValue_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::GameAnalytics_eventAddDesignEventWithValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddDesignEventWithValue)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_EventId);
	P_GET_PROPERTY(FFloatProperty,Z_Param_Value);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddDesignEventWithValue(Z_Param_EventId,Z_Param_Value,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddDesignEventWithValue

// Begin Class UGameAnalytics Function AddErrorEvent
struct Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics
{
	struct GameAnalytics_eventAddErrorEvent_Parms
	{
		EGAErrorSeverity Severity;
		FString Message;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Message_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_Severity_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Severity;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Message;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_Severity_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_Severity = { "Severity", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddErrorEvent_Parms, Severity), Z_Construct_UEnum_GameAnalytics_EGAErrorSeverity, METADATA_PARAMS(0, nullptr) }; // 772265853
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_Message = { "Message", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddErrorEvent_Parms, Message), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Message_MetaData), NewProp_Message_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddErrorEvent_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddErrorEvent_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddErrorEvent_Parms), &Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_Severity_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_Severity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_Message,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddErrorEvent", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::GameAnalytics_eventAddErrorEvent_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::GameAnalytics_eventAddErrorEvent_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddErrorEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddErrorEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddErrorEvent)
{
	P_GET_ENUM(EGAErrorSeverity,Z_Param_Severity);
	P_GET_PROPERTY(FStrProperty,Z_Param_Message);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddErrorEvent(EGAErrorSeverity(Z_Param_Severity),Z_Param_Message,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddErrorEvent

// Begin Class UGameAnalytics Function AddProgressionEvent
struct Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics
{
	struct GameAnalytics_eventAddProgressionEvent_Parms
	{
		EGAProgressionStatus ProgressionStatus;
		FString Progression01;
		FString Progression02;
		FString Progression03;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "CPP_Default_Progression02", "" },
		{ "CPP_Default_Progression03", "" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Progression01_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Progression02_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Progression03_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_ProgressionStatus_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ProgressionStatus;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Progression01;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Progression02;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Progression03;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_ProgressionStatus_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_ProgressionStatus = { "ProgressionStatus", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEvent_Parms, ProgressionStatus), Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus, METADATA_PARAMS(0, nullptr) }; // 409719171
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_Progression01 = { "Progression01", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEvent_Parms, Progression01), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Progression01_MetaData), NewProp_Progression01_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_Progression02 = { "Progression02", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEvent_Parms, Progression02), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Progression02_MetaData), NewProp_Progression02_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_Progression03 = { "Progression03", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEvent_Parms, Progression03), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Progression03_MetaData), NewProp_Progression03_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEvent_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddProgressionEvent_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddProgressionEvent_Parms), &Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_ProgressionStatus_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_ProgressionStatus,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_Progression01,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_Progression02,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_Progression03,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddProgressionEvent", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::GameAnalytics_eventAddProgressionEvent_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::GameAnalytics_eventAddProgressionEvent_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddProgressionEvent)
{
	P_GET_ENUM(EGAProgressionStatus,Z_Param_ProgressionStatus);
	P_GET_PROPERTY(FStrProperty,Z_Param_Progression01);
	P_GET_PROPERTY(FStrProperty,Z_Param_Progression02);
	P_GET_PROPERTY(FStrProperty,Z_Param_Progression03);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddProgressionEvent(EGAProgressionStatus(Z_Param_ProgressionStatus),Z_Param_Progression01,Z_Param_Progression02,Z_Param_Progression03,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddProgressionEvent

// Begin Class UGameAnalytics Function AddProgressionEventWithScore
struct Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics
{
	struct GameAnalytics_eventAddProgressionEventWithScore_Parms
	{
		EGAProgressionStatus ProgressionStatus;
		int32 Score;
		FString Progression01;
		FString Progression02;
		FString Progression03;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "CPP_Default_Progression01", "" },
		{ "CPP_Default_Progression02", "" },
		{ "CPP_Default_Progression03", "" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Progression01_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Progression02_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Progression03_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_ProgressionStatus_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ProgressionStatus;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Score;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Progression01;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Progression02;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Progression03;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_ProgressionStatus_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_ProgressionStatus = { "ProgressionStatus", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEventWithScore_Parms, ProgressionStatus), Z_Construct_UEnum_GameAnalytics_EGAProgressionStatus, METADATA_PARAMS(0, nullptr) }; // 409719171
const UECodeGen_Private::FIntPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Score = { "Score", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEventWithScore_Parms, Score), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Progression01 = { "Progression01", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEventWithScore_Parms, Progression01), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Progression01_MetaData), NewProp_Progression01_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Progression02 = { "Progression02", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEventWithScore_Parms, Progression02), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Progression02_MetaData), NewProp_Progression02_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Progression03 = { "Progression03", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEventWithScore_Parms, Progression03), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Progression03_MetaData), NewProp_Progression03_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddProgressionEventWithScore_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddProgressionEventWithScore_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddProgressionEventWithScore_Parms), &Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_ProgressionStatus_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_ProgressionStatus,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Score,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Progression01,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Progression02,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_Progression03,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddProgressionEventWithScore", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::GameAnalytics_eventAddProgressionEventWithScore_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::GameAnalytics_eventAddProgressionEventWithScore_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddProgressionEventWithScore)
{
	P_GET_ENUM(EGAProgressionStatus,Z_Param_ProgressionStatus);
	P_GET_PROPERTY(FIntProperty,Z_Param_Score);
	P_GET_PROPERTY(FStrProperty,Z_Param_Progression01);
	P_GET_PROPERTY(FStrProperty,Z_Param_Progression02);
	P_GET_PROPERTY(FStrProperty,Z_Param_Progression03);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddProgressionEventWithScore(EGAProgressionStatus(Z_Param_ProgressionStatus),Z_Param_Score,Z_Param_Progression01,Z_Param_Progression02,Z_Param_Progression03,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddProgressionEventWithScore

// Begin Class UGameAnalytics Function AddResourceEvent
struct Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics
{
	struct GameAnalytics_eventAddResourceEvent_Parms
	{
		EGAResourceFlowType FlowType;
		FString Currency;
		float Amount;
		FString ItemType;
		FString ItemId;
		FGACustomFields CustomFields;
		bool MergeFields;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_CustomFields", "()" },
		{ "CPP_Default_MergeFields", "false" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Currency_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemType_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemId_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomFields_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_FlowType_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_FlowType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Currency;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Amount;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemType;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemId;
	static const UECodeGen_Private::FStructPropertyParams NewProp_CustomFields;
	static void NewProp_MergeFields_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_MergeFields;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_FlowType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_FlowType = { "FlowType", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddResourceEvent_Parms, FlowType), Z_Construct_UEnum_GameAnalytics_EGAResourceFlowType, METADATA_PARAMS(0, nullptr) }; // 3467320194
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_Currency = { "Currency", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddResourceEvent_Parms, Currency), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Currency_MetaData), NewProp_Currency_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_Amount = { "Amount", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddResourceEvent_Parms, Amount), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_ItemType = { "ItemType", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddResourceEvent_Parms, ItemType), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemType_MetaData), NewProp_ItemType_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_ItemId = { "ItemId", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddResourceEvent_Parms, ItemId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemId_MetaData), NewProp_ItemId_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_CustomFields = { "CustomFields", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventAddResourceEvent_Parms, CustomFields), Z_Construct_UScriptStruct_FGACustomFields, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomFields_MetaData), NewProp_CustomFields_MetaData) }; // 1359929604
void Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_MergeFields_SetBit(void* Obj)
{
	((GameAnalytics_eventAddResourceEvent_Parms*)Obj)->MergeFields = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_MergeFields = { "MergeFields", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventAddResourceEvent_Parms), &Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_MergeFields_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_FlowType_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_FlowType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_Currency,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_Amount,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_ItemType,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_ItemId,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_CustomFields,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::NewProp_MergeFields,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "AddResourceEvent", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::GameAnalytics_eventAddResourceEvent_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::GameAnalytics_eventAddResourceEvent_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_AddResourceEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_AddResourceEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execAddResourceEvent)
{
	P_GET_ENUM(EGAResourceFlowType,Z_Param_FlowType);
	P_GET_PROPERTY(FStrProperty,Z_Param_Currency);
	P_GET_PROPERTY(FFloatProperty,Z_Param_Amount);
	P_GET_PROPERTY(FStrProperty,Z_Param_ItemType);
	P_GET_PROPERTY(FStrProperty,Z_Param_ItemId);
	P_GET_STRUCT(FGACustomFields,Z_Param_CustomFields);
	P_GET_UBOOL(Z_Param_MergeFields);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->AddResourceEvent(EGAResourceFlowType(Z_Param_FlowType),Z_Param_Currency,Z_Param_Amount,Z_Param_ItemType,Z_Param_ItemId,Z_Param_CustomFields,Z_Param_MergeFields);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function AddResourceEvent

// Begin Class UGameAnalytics Function ConfigureAutoDetectAppVersion
struct Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics
{
	struct GameAnalytics_eventConfigureAutoDetectAppVersion_Parms
	{
		bool Flag;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Flag_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Flag;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::NewProp_Flag_SetBit(void* Obj)
{
	((GameAnalytics_eventConfigureAutoDetectAppVersion_Parms*)Obj)->Flag = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::NewProp_Flag = { "Flag", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventConfigureAutoDetectAppVersion_Parms), &Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::NewProp_Flag_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::NewProp_Flag,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureAutoDetectAppVersion", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::GameAnalytics_eventConfigureAutoDetectAppVersion_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::GameAnalytics_eventConfigureAutoDetectAppVersion_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureAutoDetectAppVersion)
{
	P_GET_UBOOL(Z_Param_Flag);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureAutoDetectAppVersion(Z_Param_Flag);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureAutoDetectAppVersion

// Begin Class UGameAnalytics Function ConfigureAvailableCustomDimensions01
struct Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics
{
	struct GameAnalytics_eventConfigureAvailableCustomDimensions01_Parms
	{
		TArray<FString> List;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_List_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_List_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_List;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::NewProp_List_Inner = { "List", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::NewProp_List = { "List", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureAvailableCustomDimensions01_Parms, List), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_List_MetaData), NewProp_List_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::NewProp_List_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::NewProp_List,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureAvailableCustomDimensions01", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::GameAnalytics_eventConfigureAvailableCustomDimensions01_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::GameAnalytics_eventConfigureAvailableCustomDimensions01_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureAvailableCustomDimensions01)
{
	P_GET_TARRAY_REF(FString,Z_Param_Out_List);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureAvailableCustomDimensions01(Z_Param_Out_List);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureAvailableCustomDimensions01

// Begin Class UGameAnalytics Function ConfigureAvailableCustomDimensions02
struct Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics
{
	struct GameAnalytics_eventConfigureAvailableCustomDimensions02_Parms
	{
		TArray<FString> List;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_List_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_List_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_List;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::NewProp_List_Inner = { "List", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::NewProp_List = { "List", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureAvailableCustomDimensions02_Parms, List), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_List_MetaData), NewProp_List_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::NewProp_List_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::NewProp_List,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureAvailableCustomDimensions02", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::GameAnalytics_eventConfigureAvailableCustomDimensions02_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::GameAnalytics_eventConfigureAvailableCustomDimensions02_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureAvailableCustomDimensions02)
{
	P_GET_TARRAY_REF(FString,Z_Param_Out_List);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureAvailableCustomDimensions02(Z_Param_Out_List);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureAvailableCustomDimensions02

// Begin Class UGameAnalytics Function ConfigureAvailableCustomDimensions03
struct Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics
{
	struct GameAnalytics_eventConfigureAvailableCustomDimensions03_Parms
	{
		TArray<FString> List;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_List_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_List_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_List;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::NewProp_List_Inner = { "List", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::NewProp_List = { "List", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureAvailableCustomDimensions03_Parms, List), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_List_MetaData), NewProp_List_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::NewProp_List_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::NewProp_List,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureAvailableCustomDimensions03", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::GameAnalytics_eventConfigureAvailableCustomDimensions03_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::GameAnalytics_eventConfigureAvailableCustomDimensions03_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureAvailableCustomDimensions03)
{
	P_GET_TARRAY_REF(FString,Z_Param_Out_List);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureAvailableCustomDimensions03(Z_Param_Out_List);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureAvailableCustomDimensions03

// Begin Class UGameAnalytics Function ConfigureAvailableResourceCurrencies
struct Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics
{
	struct GameAnalytics_eventConfigureAvailableResourceCurrencies_Parms
	{
		TArray<FString> List;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_List_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_List_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_List;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::NewProp_List_Inner = { "List", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::NewProp_List = { "List", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureAvailableResourceCurrencies_Parms, List), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_List_MetaData), NewProp_List_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::NewProp_List_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::NewProp_List,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureAvailableResourceCurrencies", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::GameAnalytics_eventConfigureAvailableResourceCurrencies_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::GameAnalytics_eventConfigureAvailableResourceCurrencies_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureAvailableResourceCurrencies)
{
	P_GET_TARRAY_REF(FString,Z_Param_Out_List);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureAvailableResourceCurrencies(Z_Param_Out_List);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureAvailableResourceCurrencies

// Begin Class UGameAnalytics Function ConfigureAvailableResourceItemTypes
struct Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics
{
	struct GameAnalytics_eventConfigureAvailableResourceItemTypes_Parms
	{
		TArray<FString> List;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_List_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_List_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_List;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::NewProp_List_Inner = { "List", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::NewProp_List = { "List", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureAvailableResourceItemTypes_Parms, List), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_List_MetaData), NewProp_List_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::NewProp_List_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::NewProp_List,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureAvailableResourceItemTypes", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::GameAnalytics_eventConfigureAvailableResourceItemTypes_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::GameAnalytics_eventConfigureAvailableResourceItemTypes_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureAvailableResourceItemTypes)
{
	P_GET_TARRAY_REF(FString,Z_Param_Out_List);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureAvailableResourceItemTypes(Z_Param_Out_List);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureAvailableResourceItemTypes

// Begin Class UGameAnalytics Function ConfigureBuild
struct Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics
{
	struct GameAnalytics_eventConfigureBuild_Parms
	{
		FString Build;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Build_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Build;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::NewProp_Build = { "Build", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureBuild_Parms, Build), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Build_MetaData), NewProp_Build_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::NewProp_Build,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureBuild", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::GameAnalytics_eventConfigureBuild_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::GameAnalytics_eventConfigureBuild_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureBuild()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureBuild_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureBuild)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Build);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureBuild(Z_Param_Out_Build);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureBuild

// Begin Class UGameAnalytics Function ConfigureExternalUserId
struct Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics
{
	struct GameAnalytics_eventConfigureExternalUserId_Parms
	{
		FString UserId;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UserId_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_UserId;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::NewProp_UserId = { "UserId", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureExternalUserId_Parms, UserId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UserId_MetaData), NewProp_UserId_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::NewProp_UserId,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureExternalUserId", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::GameAnalytics_eventConfigureExternalUserId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::GameAnalytics_eventConfigureExternalUserId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureExternalUserId)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_UserId);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureExternalUserId(Z_Param_Out_UserId);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureExternalUserId

// Begin Class UGameAnalytics Function ConfigureGameEngineVersion
struct Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics
{
	struct GameAnalytics_eventConfigureGameEngineVersion_Parms
	{
		FString GameEngineVersion;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_GameEngineVersion_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_GameEngineVersion;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::NewProp_GameEngineVersion = { "GameEngineVersion", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureGameEngineVersion_Parms, GameEngineVersion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_GameEngineVersion_MetaData), NewProp_GameEngineVersion_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::NewProp_GameEngineVersion,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureGameEngineVersion", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::GameAnalytics_eventConfigureGameEngineVersion_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::GameAnalytics_eventConfigureGameEngineVersion_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureGameEngineVersion)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_GameEngineVersion);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureGameEngineVersion(Z_Param_GameEngineVersion);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureGameEngineVersion

// Begin Class UGameAnalytics Function ConfigureSdkGameEngineVersion
struct Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics
{
	struct GameAnalytics_eventConfigureSdkGameEngineVersion_Parms
	{
		FString GameEngineSdkVersion;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_GameEngineSdkVersion_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_GameEngineSdkVersion;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::NewProp_GameEngineSdkVersion = { "GameEngineSdkVersion", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureSdkGameEngineVersion_Parms, GameEngineSdkVersion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_GameEngineSdkVersion_MetaData), NewProp_GameEngineSdkVersion_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::NewProp_GameEngineSdkVersion,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureSdkGameEngineVersion", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::GameAnalytics_eventConfigureSdkGameEngineVersion_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::GameAnalytics_eventConfigureSdkGameEngineVersion_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureSdkGameEngineVersion)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_GameEngineSdkVersion);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureSdkGameEngineVersion(Z_Param_GameEngineSdkVersion);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureSdkGameEngineVersion

// Begin Class UGameAnalytics Function ConfigureUserId
struct Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics
{
	struct GameAnalytics_eventConfigureUserId_Parms
	{
		FString UserId;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UserId_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_UserId;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::NewProp_UserId = { "UserId", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventConfigureUserId_Parms, UserId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UserId_MetaData), NewProp_UserId_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::NewProp_UserId,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "ConfigureUserId", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::GameAnalytics_eventConfigureUserId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::GameAnalytics_eventConfigureUserId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_ConfigureUserId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_ConfigureUserId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execConfigureUserId)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_UserId);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ConfigureUserId(Z_Param_Out_UserId);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function ConfigureUserId

// Begin Class UGameAnalytics Function DisableDeviceInfo
struct Z_Construct_UFunction_UGameAnalytics_DisableDeviceInfo_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_DisableDeviceInfo_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "DisableDeviceInfo", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_DisableDeviceInfo_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_DisableDeviceInfo_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UFunction_UGameAnalytics_DisableDeviceInfo()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_DisableDeviceInfo_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execDisableDeviceInfo)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->DisableDeviceInfo();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function DisableDeviceInfo

// Begin Class UGameAnalytics Function EnableAdvertisingId
struct Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics
{
	struct GameAnalytics_eventEnableAdvertisingId_Parms
	{
		bool Value;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Value_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Value;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::NewProp_Value_SetBit(void* Obj)
{
	((GameAnalytics_eventEnableAdvertisingId_Parms*)Obj)->Value = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventEnableAdvertisingId_Parms), &Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::NewProp_Value_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::NewProp_Value,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "EnableAdvertisingId", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::GameAnalytics_eventEnableAdvertisingId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::GameAnalytics_eventEnableAdvertisingId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execEnableAdvertisingId)
{
	P_GET_UBOOL(Z_Param_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->EnableAdvertisingId(Z_Param_Value);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function EnableAdvertisingId

// Begin Class UGameAnalytics Function EnableFpsHistogram
struct Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics
{
	struct GameAnalytics_eventEnableFpsHistogram_Parms
	{
		bool Value;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Value_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Value;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::NewProp_Value_SetBit(void* Obj)
{
	((GameAnalytics_eventEnableFpsHistogram_Parms*)Obj)->Value = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventEnableFpsHistogram_Parms), &Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::NewProp_Value_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::NewProp_Value,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "EnableFpsHistogram", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::GameAnalytics_eventEnableFpsHistogram_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::GameAnalytics_eventEnableFpsHistogram_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execEnableFpsHistogram)
{
	P_GET_UBOOL(Z_Param_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->EnableFpsHistogram(Z_Param_Value);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function EnableFpsHistogram

// Begin Class UGameAnalytics Function EnableHealthHardwareInfo
struct Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics
{
	struct GameAnalytics_eventEnableHealthHardwareInfo_Parms
	{
		bool Value;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Value_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Value;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::NewProp_Value_SetBit(void* Obj)
{
	((GameAnalytics_eventEnableHealthHardwareInfo_Parms*)Obj)->Value = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventEnableHealthHardwareInfo_Parms), &Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::NewProp_Value_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::NewProp_Value,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "EnableHealthHardwareInfo", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::GameAnalytics_eventEnableHealthHardwareInfo_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::GameAnalytics_eventEnableHealthHardwareInfo_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execEnableHealthHardwareInfo)
{
	P_GET_UBOOL(Z_Param_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->EnableHealthHardwareInfo(Z_Param_Value);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function EnableHealthHardwareInfo

// Begin Class UGameAnalytics Function EnableMemoryHistogram
struct Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics
{
	struct GameAnalytics_eventEnableMemoryHistogram_Parms
	{
		bool Value;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Value_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Value;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::NewProp_Value_SetBit(void* Obj)
{
	((GameAnalytics_eventEnableMemoryHistogram_Parms*)Obj)->Value = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventEnableMemoryHistogram_Parms), &Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::NewProp_Value_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::NewProp_Value,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "EnableMemoryHistogram", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::GameAnalytics_eventEnableMemoryHistogram_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::GameAnalytics_eventEnableMemoryHistogram_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execEnableMemoryHistogram)
{
	P_GET_UBOOL(Z_Param_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->EnableMemoryHistogram(Z_Param_Value);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function EnableMemoryHistogram

// Begin Class UGameAnalytics Function EnableSDKInitEvent
struct Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics
{
	struct GameAnalytics_eventEnableSDKInitEvent_Parms
	{
		bool Value;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "////////////////////////////////////////////////////////////\n// HEALTH EVENT\n////////////////////////////////////////////////////////////\n" },
#endif
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "HEALTH EVENT" },
#endif
	};
#endif // WITH_METADATA
	static void NewProp_Value_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Value;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::NewProp_Value_SetBit(void* Obj)
{
	((GameAnalytics_eventEnableSDKInitEvent_Parms*)Obj)->Value = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventEnableSDKInitEvent_Parms), &Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::NewProp_Value_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::NewProp_Value,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "EnableSDKInitEvent", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::GameAnalytics_eventEnableSDKInitEvent_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::GameAnalytics_eventEnableSDKInitEvent_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execEnableSDKInitEvent)
{
	P_GET_UBOOL(Z_Param_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->EnableSDKInitEvent(Z_Param_Value);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function EnableSDKInitEvent

// Begin Class UGameAnalytics Function EndSession
struct Z_Construct_UFunction_UGameAnalytics_EndSession_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_EndSession_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "EndSession", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_EndSession_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_EndSession_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UFunction_UGameAnalytics_EndSession()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_EndSession_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execEndSession)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->EndSession();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function EndSession

// Begin Class UGameAnalytics Function GetABTestingId
struct Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics
{
	struct GameAnalytics_eventGetABTestingId_Parms
	{
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetABTestingId_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetABTestingId", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::GameAnalytics_eventGetABTestingId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::GameAnalytics_eventGetABTestingId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetABTestingId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetABTestingId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetABTestingId)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=P_THIS->GetABTestingId();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetABTestingId

// Begin Class UGameAnalytics Function GetABTestingVariantId
struct Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics
{
	struct GameAnalytics_eventGetABTestingVariantId_Parms
	{
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetABTestingVariantId_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetABTestingVariantId", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::GameAnalytics_eventGetABTestingVariantId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::GameAnalytics_eventGetABTestingVariantId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetABTestingVariantId)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=P_THIS->GetABTestingVariantId();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetABTestingVariantId

// Begin Class UGameAnalytics Function GetElapsedSessionTime
struct Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics
{
	struct GameAnalytics_eventGetElapsedSessionTime_Parms
	{
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "////////////////////////////////////////////////////////////\n" },
#endif
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetElapsedSessionTime_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetElapsedSessionTime", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::GameAnalytics_eventGetElapsedSessionTime_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::GameAnalytics_eventGetElapsedSessionTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetElapsedSessionTime)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=P_THIS->GetElapsedSessionTime();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetElapsedSessionTime

// Begin Class UGameAnalytics Function GetElapsedTimeFromAllSessions
struct Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics
{
	struct GameAnalytics_eventGetElapsedTimeFromAllSessions_Parms
	{
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetElapsedTimeFromAllSessions_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetElapsedTimeFromAllSessions", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::GameAnalytics_eventGetElapsedTimeFromAllSessions_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::GameAnalytics_eventGetElapsedTimeFromAllSessions_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetElapsedTimeFromAllSessions)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=P_THIS->GetElapsedTimeFromAllSessions();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetElapsedTimeFromAllSessions

// Begin Class UGameAnalytics Function GetExternalUserId
struct Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics
{
	struct GameAnalytics_eventGetExternalUserId_Parms
	{
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetExternalUserId_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetExternalUserId", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::GameAnalytics_eventGetExternalUserId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::GameAnalytics_eventGetExternalUserId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetExternalUserId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetExternalUserId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetExternalUserId)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=P_THIS->GetExternalUserId();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetExternalUserId

// Begin Class UGameAnalytics Function GetInstance
struct Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics
{
	struct GameAnalytics_eventGetInstance_Parms
	{
		UGameAnalytics* ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FObjectPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetInstance_Parms, ReturnValue), Z_Construct_UClass_UGameAnalytics_NoRegister, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetInstance", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::GameAnalytics_eventGetInstance_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::GameAnalytics_eventGetInstance_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetInstance()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetInstance_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetInstance)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(UGameAnalytics**)Z_Param__Result=UGameAnalytics::GetInstance();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetInstance

// Begin Class UGameAnalytics Function GetRemoteConfigsContentAsString
struct Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics
{
	struct GameAnalytics_eventGetRemoteConfigsContentAsString_Parms
	{
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetRemoteConfigsContentAsString_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetRemoteConfigsContentAsString", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::GameAnalytics_eventGetRemoteConfigsContentAsString_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::GameAnalytics_eventGetRemoteConfigsContentAsString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetRemoteConfigsContentAsString)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=P_THIS->GetRemoteConfigsContentAsString();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetRemoteConfigsContentAsString

// Begin Class UGameAnalytics Function GetRemoteConfigsValueAsString
struct Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics
{
	struct GameAnalytics_eventGetRemoteConfigsValueAsString_Parms
	{
		FString Key;
		FString DefaultValue;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "CPP_Default_DefaultValue", "" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Key_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DefaultValue_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Key;
	static const UECodeGen_Private::FStrPropertyParams NewProp_DefaultValue;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::NewProp_Key = { "Key", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetRemoteConfigsValueAsString_Parms, Key), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Key_MetaData), NewProp_Key_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::NewProp_DefaultValue = { "DefaultValue", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetRemoteConfigsValueAsString_Parms, DefaultValue), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DefaultValue_MetaData), NewProp_DefaultValue_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetRemoteConfigsValueAsString_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::NewProp_Key,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::NewProp_DefaultValue,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetRemoteConfigsValueAsString", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::GameAnalytics_eventGetRemoteConfigsValueAsString_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::GameAnalytics_eventGetRemoteConfigsValueAsString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetRemoteConfigsValueAsString)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_Key);
	P_GET_PROPERTY(FStrProperty,Z_Param_DefaultValue);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=P_THIS->GetRemoteConfigsValueAsString(Z_Param_Key,Z_Param_DefaultValue);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetRemoteConfigsValueAsString

// Begin Class UGameAnalytics Function GetUserId
struct Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics
{
	struct GameAnalytics_eventGetUserId_Parms
	{
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventGetUserId_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "GetUserId", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::GameAnalytics_eventGetUserId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::GameAnalytics_eventGetUserId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_GetUserId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_GetUserId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execGetUserId)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=P_THIS->GetUserId();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function GetUserId

// Begin Class UGameAnalytics Function Initialize
struct Z_Construct_UFunction_UGameAnalytics_Initialize_Statics
{
	struct GameAnalytics_eventInitialize_Parms
	{
		FString GameKey;
		FString GameSecret;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_GameKey_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_GameSecret_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_GameKey;
	static const UECodeGen_Private::FStrPropertyParams NewProp_GameSecret;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::NewProp_GameKey = { "GameKey", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventInitialize_Parms, GameKey), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_GameKey_MetaData), NewProp_GameKey_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::NewProp_GameSecret = { "GameSecret", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventInitialize_Parms, GameSecret), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_GameSecret_MetaData), NewProp_GameSecret_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::NewProp_GameKey,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::NewProp_GameSecret,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "Initialize", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::GameAnalytics_eventInitialize_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::GameAnalytics_eventInitialize_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_Initialize()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_Initialize_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execInitialize)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_GameKey);
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_GameSecret);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Initialize(Z_Param_Out_GameKey,Z_Param_Out_GameSecret);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function Initialize

// Begin Class UGameAnalytics Function IsRemoteConfigsReady
struct Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics
{
	struct GameAnalytics_eventIsRemoteConfigsReady_Parms
	{
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_ReturnValue_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::NewProp_ReturnValue_SetBit(void* Obj)
{
	((GameAnalytics_eventIsRemoteConfigsReady_Parms*)Obj)->ReturnValue = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventIsRemoteConfigsReady_Parms), &Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "IsRemoteConfigsReady", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::GameAnalytics_eventIsRemoteConfigsReady_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::GameAnalytics_eventIsRemoteConfigsReady_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execIsRemoteConfigsReady)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->IsRemoteConfigsReady();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function IsRemoteConfigsReady

// Begin Class UGameAnalytics Function OnQuit
struct Z_Construct_UFunction_UGameAnalytics_OnQuit_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_OnQuit_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "OnQuit", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_OnQuit_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_OnQuit_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UFunction_UGameAnalytics_OnQuit()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_OnQuit_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execOnQuit)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->OnQuit();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function OnQuit

// Begin Class UGameAnalytics Function SetCustomDimension01
struct Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics
{
	struct GameAnalytics_eventSetCustomDimension01_Parms
	{
		FString CustomDimension;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "////////////////////////////////////////////////////////////\n" },
#endif
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomDimension_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_CustomDimension;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::NewProp_CustomDimension = { "CustomDimension", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventSetCustomDimension01_Parms, CustomDimension), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomDimension_MetaData), NewProp_CustomDimension_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::NewProp_CustomDimension,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetCustomDimension01", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::GameAnalytics_eventSetCustomDimension01_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::GameAnalytics_eventSetCustomDimension01_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetCustomDimension01)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_CustomDimension);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetCustomDimension01(Z_Param_CustomDimension);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetCustomDimension01

// Begin Class UGameAnalytics Function SetCustomDimension02
struct Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics
{
	struct GameAnalytics_eventSetCustomDimension02_Parms
	{
		FString CustomDimension;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomDimension_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_CustomDimension;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::NewProp_CustomDimension = { "CustomDimension", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventSetCustomDimension02_Parms, CustomDimension), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomDimension_MetaData), NewProp_CustomDimension_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::NewProp_CustomDimension,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetCustomDimension02", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::GameAnalytics_eventSetCustomDimension02_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::GameAnalytics_eventSetCustomDimension02_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetCustomDimension02)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_CustomDimension);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetCustomDimension02(Z_Param_CustomDimension);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetCustomDimension02

// Begin Class UGameAnalytics Function SetCustomDimension03
struct Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics
{
	struct GameAnalytics_eventSetCustomDimension03_Parms
	{
		FString CustomDimension;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CustomDimension_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_CustomDimension;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::NewProp_CustomDimension = { "CustomDimension", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventSetCustomDimension03_Parms, CustomDimension), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CustomDimension_MetaData), NewProp_CustomDimension_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::NewProp_CustomDimension,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetCustomDimension03", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::GameAnalytics_eventSetCustomDimension03_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::GameAnalytics_eventSetCustomDimension03_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetCustomDimension03)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_CustomDimension);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetCustomDimension03(Z_Param_CustomDimension);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetCustomDimension03

// Begin Class UGameAnalytics Function SetEnabledErrorReporting
struct Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics
{
	struct GameAnalytics_eventSetEnabledErrorReporting_Parms
	{
		bool Flag;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Flag_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Flag;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::NewProp_Flag_SetBit(void* Obj)
{
	((GameAnalytics_eventSetEnabledErrorReporting_Parms*)Obj)->Flag = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::NewProp_Flag = { "Flag", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventSetEnabledErrorReporting_Parms), &Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::NewProp_Flag_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::NewProp_Flag,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetEnabledErrorReporting", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::GameAnalytics_eventSetEnabledErrorReporting_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::GameAnalytics_eventSetEnabledErrorReporting_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetEnabledErrorReporting)
{
	P_GET_UBOOL(Z_Param_Flag);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetEnabledErrorReporting(Z_Param_Flag);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetEnabledErrorReporting

// Begin Class UGameAnalytics Function SetEnabledEventSubmission
struct Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics
{
	struct GameAnalytics_eventSetEnabledEventSubmission_Parms
	{
		bool Flag;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Flag_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Flag;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::NewProp_Flag_SetBit(void* Obj)
{
	((GameAnalytics_eventSetEnabledEventSubmission_Parms*)Obj)->Flag = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::NewProp_Flag = { "Flag", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventSetEnabledEventSubmission_Parms), &Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::NewProp_Flag_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::NewProp_Flag,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetEnabledEventSubmission", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::GameAnalytics_eventSetEnabledEventSubmission_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::GameAnalytics_eventSetEnabledEventSubmission_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetEnabledEventSubmission)
{
	P_GET_UBOOL(Z_Param_Flag);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetEnabledEventSubmission(Z_Param_Flag);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetEnabledEventSubmission

// Begin Class UGameAnalytics Function SetEnabledInfoLog
struct Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics
{
	struct GameAnalytics_eventSetEnabledInfoLog_Parms
	{
		bool Flag;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Flag_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Flag;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::NewProp_Flag_SetBit(void* Obj)
{
	((GameAnalytics_eventSetEnabledInfoLog_Parms*)Obj)->Flag = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::NewProp_Flag = { "Flag", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventSetEnabledInfoLog_Parms), &Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::NewProp_Flag_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::NewProp_Flag,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetEnabledInfoLog", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::GameAnalytics_eventSetEnabledInfoLog_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::GameAnalytics_eventSetEnabledInfoLog_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetEnabledInfoLog)
{
	P_GET_UBOOL(Z_Param_Flag);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetEnabledInfoLog(Z_Param_Flag);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetEnabledInfoLog

// Begin Class UGameAnalytics Function SetEnabledManualSessionHandling
struct Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics
{
	struct GameAnalytics_eventSetEnabledManualSessionHandling_Parms
	{
		bool Flag;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Flag_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Flag;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::NewProp_Flag_SetBit(void* Obj)
{
	((GameAnalytics_eventSetEnabledManualSessionHandling_Parms*)Obj)->Flag = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::NewProp_Flag = { "Flag", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventSetEnabledManualSessionHandling_Parms), &Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::NewProp_Flag_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::NewProp_Flag,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetEnabledManualSessionHandling", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::GameAnalytics_eventSetEnabledManualSessionHandling_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::GameAnalytics_eventSetEnabledManualSessionHandling_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetEnabledManualSessionHandling)
{
	P_GET_UBOOL(Z_Param_Flag);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetEnabledManualSessionHandling(Z_Param_Flag);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetEnabledManualSessionHandling

// Begin Class UGameAnalytics Function SetEnabledVerboseLog
struct Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics
{
	struct GameAnalytics_eventSetEnabledVerboseLog_Parms
	{
		bool Flag;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static void NewProp_Flag_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_Flag;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
void Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::NewProp_Flag_SetBit(void* Obj)
{
	((GameAnalytics_eventSetEnabledVerboseLog_Parms*)Obj)->Flag = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::NewProp_Flag = { "Flag", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(GameAnalytics_eventSetEnabledVerboseLog_Parms), &Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::NewProp_Flag_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::NewProp_Flag,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetEnabledVerboseLog", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::GameAnalytics_eventSetEnabledVerboseLog_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::GameAnalytics_eventSetEnabledVerboseLog_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetEnabledVerboseLog)
{
	P_GET_UBOOL(Z_Param_Flag);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetEnabledVerboseLog(Z_Param_Flag);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetEnabledVerboseLog

// Begin Class UGameAnalytics Function SetWritablePath
struct Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics
{
	struct GameAnalytics_eventSetWritablePath_Parms
	{
		FString Path;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "////////////////////////////////////////////////////////////\n" },
#endif
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Path_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Path;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::NewProp_Path = { "Path", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameAnalytics_eventSetWritablePath_Parms, Path), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Path_MetaData), NewProp_Path_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::NewProp_Path,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "SetWritablePath", nullptr, nullptr, Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::PropPointers), sizeof(Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::GameAnalytics_eventSetWritablePath_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::GameAnalytics_eventSetWritablePath_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameAnalytics_SetWritablePath()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_SetWritablePath_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execSetWritablePath)
{
	P_GET_PROPERTY_REF(FStrProperty,Z_Param_Out_Path);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetWritablePath(Z_Param_Out_Path);
	P_NATIVE_END;
}
// End Class UGameAnalytics Function SetWritablePath

// Begin Class UGameAnalytics Function StartSession
struct Z_Construct_UFunction_UGameAnalytics_StartSession_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "GameAnalytics" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameAnalytics_StartSession_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameAnalytics, nullptr, "StartSession", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameAnalytics_StartSession_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameAnalytics_StartSession_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UFunction_UGameAnalytics_StartSession()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameAnalytics_StartSession_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameAnalytics::execStartSession)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->StartSession();
	P_NATIVE_END;
}
// End Class UGameAnalytics Function StartSession

// Begin Class UGameAnalytics
void UGameAnalytics::StaticRegisterNativesUGameAnalytics()
{
	UClass* Class = UGameAnalytics::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "AddAdEvent", &UGameAnalytics::execAddAdEvent },
		{ "AddAdEventWithDuration", &UGameAnalytics::execAddAdEventWithDuration },
		{ "AddAdEventWithReason", &UGameAnalytics::execAddAdEventWithReason },
		{ "AddBusinessEvent", &UGameAnalytics::execAddBusinessEvent },
		{ "AddBusinessEventAndAutoFetchReceipt", &UGameAnalytics::execAddBusinessEventAndAutoFetchReceipt },
		{ "AddBusinessEventWithReceipt", &UGameAnalytics::execAddBusinessEventWithReceipt },
		{ "AddDesignEvent", &UGameAnalytics::execAddDesignEvent },
		{ "AddDesignEventWithValue", &UGameAnalytics::execAddDesignEventWithValue },
		{ "AddErrorEvent", &UGameAnalytics::execAddErrorEvent },
		{ "AddProgressionEvent", &UGameAnalytics::execAddProgressionEvent },
		{ "AddProgressionEventWithScore", &UGameAnalytics::execAddProgressionEventWithScore },
		{ "AddResourceEvent", &UGameAnalytics::execAddResourceEvent },
		{ "ConfigureAutoDetectAppVersion", &UGameAnalytics::execConfigureAutoDetectAppVersion },
		{ "ConfigureAvailableCustomDimensions01", &UGameAnalytics::execConfigureAvailableCustomDimensions01 },
		{ "ConfigureAvailableCustomDimensions02", &UGameAnalytics::execConfigureAvailableCustomDimensions02 },
		{ "ConfigureAvailableCustomDimensions03", &UGameAnalytics::execConfigureAvailableCustomDimensions03 },
		{ "ConfigureAvailableResourceCurrencies", &UGameAnalytics::execConfigureAvailableResourceCurrencies },
		{ "ConfigureAvailableResourceItemTypes", &UGameAnalytics::execConfigureAvailableResourceItemTypes },
		{ "ConfigureBuild", &UGameAnalytics::execConfigureBuild },
		{ "ConfigureExternalUserId", &UGameAnalytics::execConfigureExternalUserId },
		{ "ConfigureGameEngineVersion", &UGameAnalytics::execConfigureGameEngineVersion },
		{ "ConfigureSdkGameEngineVersion", &UGameAnalytics::execConfigureSdkGameEngineVersion },
		{ "ConfigureUserId", &UGameAnalytics::execConfigureUserId },
		{ "DisableDeviceInfo", &UGameAnalytics::execDisableDeviceInfo },
		{ "EnableAdvertisingId", &UGameAnalytics::execEnableAdvertisingId },
		{ "EnableFpsHistogram", &UGameAnalytics::execEnableFpsHistogram },
		{ "EnableHealthHardwareInfo", &UGameAnalytics::execEnableHealthHardwareInfo },
		{ "EnableMemoryHistogram", &UGameAnalytics::execEnableMemoryHistogram },
		{ "EnableSDKInitEvent", &UGameAnalytics::execEnableSDKInitEvent },
		{ "EndSession", &UGameAnalytics::execEndSession },
		{ "GetABTestingId", &UGameAnalytics::execGetABTestingId },
		{ "GetABTestingVariantId", &UGameAnalytics::execGetABTestingVariantId },
		{ "GetElapsedSessionTime", &UGameAnalytics::execGetElapsedSessionTime },
		{ "GetElapsedTimeFromAllSessions", &UGameAnalytics::execGetElapsedTimeFromAllSessions },
		{ "GetExternalUserId", &UGameAnalytics::execGetExternalUserId },
		{ "GetInstance", &UGameAnalytics::execGetInstance },
		{ "GetRemoteConfigsContentAsString", &UGameAnalytics::execGetRemoteConfigsContentAsString },
		{ "GetRemoteConfigsValueAsString", &UGameAnalytics::execGetRemoteConfigsValueAsString },
		{ "GetUserId", &UGameAnalytics::execGetUserId },
		{ "Initialize", &UGameAnalytics::execInitialize },
		{ "IsRemoteConfigsReady", &UGameAnalytics::execIsRemoteConfigsReady },
		{ "OnQuit", &UGameAnalytics::execOnQuit },
		{ "SetCustomDimension01", &UGameAnalytics::execSetCustomDimension01 },
		{ "SetCustomDimension02", &UGameAnalytics::execSetCustomDimension02 },
		{ "SetCustomDimension03", &UGameAnalytics::execSetCustomDimension03 },
		{ "SetEnabledErrorReporting", &UGameAnalytics::execSetEnabledErrorReporting },
		{ "SetEnabledEventSubmission", &UGameAnalytics::execSetEnabledEventSubmission },
		{ "SetEnabledInfoLog", &UGameAnalytics::execSetEnabledInfoLog },
		{ "SetEnabledManualSessionHandling", &UGameAnalytics::execSetEnabledManualSessionHandling },
		{ "SetEnabledVerboseLog", &UGameAnalytics::execSetEnabledVerboseLog },
		{ "SetWritablePath", &UGameAnalytics::execSetWritablePath },
		{ "StartSession", &UGameAnalytics::execStartSession },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UGameAnalytics);
UClass* Z_Construct_UClass_UGameAnalytics_NoRegister()
{
	return UGameAnalytics::StaticClass();
}
struct Z_Construct_UClass_UGameAnalytics_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "GameAnalytics.h" },
		{ "ModuleRelativePath", "Public/GameAnalytics.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UGameAnalytics_AddAdEvent, "AddAdEvent" }, // 4281264457
		{ &Z_Construct_UFunction_UGameAnalytics_AddAdEventWithDuration, "AddAdEventWithDuration" }, // 3966852587
		{ &Z_Construct_UFunction_UGameAnalytics_AddAdEventWithReason, "AddAdEventWithReason" }, // 4141305780
		{ &Z_Construct_UFunction_UGameAnalytics_AddBusinessEvent, "AddBusinessEvent" }, // 3187276339
		{ &Z_Construct_UFunction_UGameAnalytics_AddBusinessEventAndAutoFetchReceipt, "AddBusinessEventAndAutoFetchReceipt" }, // 3354352547
		{ &Z_Construct_UFunction_UGameAnalytics_AddBusinessEventWithReceipt, "AddBusinessEventWithReceipt" }, // 4147550336
		{ &Z_Construct_UFunction_UGameAnalytics_AddDesignEvent, "AddDesignEvent" }, // 3427715209
		{ &Z_Construct_UFunction_UGameAnalytics_AddDesignEventWithValue, "AddDesignEventWithValue" }, // 1874364113
		{ &Z_Construct_UFunction_UGameAnalytics_AddErrorEvent, "AddErrorEvent" }, // 3134560986
		{ &Z_Construct_UFunction_UGameAnalytics_AddProgressionEvent, "AddProgressionEvent" }, // 1207489483
		{ &Z_Construct_UFunction_UGameAnalytics_AddProgressionEventWithScore, "AddProgressionEventWithScore" }, // 1454179789
		{ &Z_Construct_UFunction_UGameAnalytics_AddResourceEvent, "AddResourceEvent" }, // 1109991592
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureAutoDetectAppVersion, "ConfigureAutoDetectAppVersion" }, // 2935372198
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions01, "ConfigureAvailableCustomDimensions01" }, // 1836933116
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions02, "ConfigureAvailableCustomDimensions02" }, // 1937334135
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableCustomDimensions03, "ConfigureAvailableCustomDimensions03" }, // 1794823376
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceCurrencies, "ConfigureAvailableResourceCurrencies" }, // 1074207269
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureAvailableResourceItemTypes, "ConfigureAvailableResourceItemTypes" }, // 1180774459
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureBuild, "ConfigureBuild" }, // 2947680793
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureExternalUserId, "ConfigureExternalUserId" }, // 1015054551
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureGameEngineVersion, "ConfigureGameEngineVersion" }, // 1527516550
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureSdkGameEngineVersion, "ConfigureSdkGameEngineVersion" }, // 2837912382
		{ &Z_Construct_UFunction_UGameAnalytics_ConfigureUserId, "ConfigureUserId" }, // 900049652
		{ &Z_Construct_UFunction_UGameAnalytics_DisableDeviceInfo, "DisableDeviceInfo" }, // 424396148
		{ &Z_Construct_UFunction_UGameAnalytics_EnableAdvertisingId, "EnableAdvertisingId" }, // 2667266796
		{ &Z_Construct_UFunction_UGameAnalytics_EnableFpsHistogram, "EnableFpsHistogram" }, // 67625079
		{ &Z_Construct_UFunction_UGameAnalytics_EnableHealthHardwareInfo, "EnableHealthHardwareInfo" }, // 675839712
		{ &Z_Construct_UFunction_UGameAnalytics_EnableMemoryHistogram, "EnableMemoryHistogram" }, // 4185754290
		{ &Z_Construct_UFunction_UGameAnalytics_EnableSDKInitEvent, "EnableSDKInitEvent" }, // 3962530388
		{ &Z_Construct_UFunction_UGameAnalytics_EndSession, "EndSession" }, // 782158633
		{ &Z_Construct_UFunction_UGameAnalytics_GetABTestingId, "GetABTestingId" }, // 1918458656
		{ &Z_Construct_UFunction_UGameAnalytics_GetABTestingVariantId, "GetABTestingVariantId" }, // 256867530
		{ &Z_Construct_UFunction_UGameAnalytics_GetElapsedSessionTime, "GetElapsedSessionTime" }, // 2598195864
		{ &Z_Construct_UFunction_UGameAnalytics_GetElapsedTimeFromAllSessions, "GetElapsedTimeFromAllSessions" }, // 3790967118
		{ &Z_Construct_UFunction_UGameAnalytics_GetExternalUserId, "GetExternalUserId" }, // 4243143713
		{ &Z_Construct_UFunction_UGameAnalytics_GetInstance, "GetInstance" }, // 3986259566
		{ &Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsContentAsString, "GetRemoteConfigsContentAsString" }, // 3188976705
		{ &Z_Construct_UFunction_UGameAnalytics_GetRemoteConfigsValueAsString, "GetRemoteConfigsValueAsString" }, // 2119978109
		{ &Z_Construct_UFunction_UGameAnalytics_GetUserId, "GetUserId" }, // 1075107056
		{ &Z_Construct_UFunction_UGameAnalytics_Initialize, "Initialize" }, // 3870445673
		{ &Z_Construct_UFunction_UGameAnalytics_IsRemoteConfigsReady, "IsRemoteConfigsReady" }, // 3376303471
		{ &Z_Construct_UFunction_UGameAnalytics_OnQuit, "OnQuit" }, // 2974379947
		{ &Z_Construct_UFunction_UGameAnalytics_SetCustomDimension01, "SetCustomDimension01" }, // 1218435833
		{ &Z_Construct_UFunction_UGameAnalytics_SetCustomDimension02, "SetCustomDimension02" }, // 1718770064
		{ &Z_Construct_UFunction_UGameAnalytics_SetCustomDimension03, "SetCustomDimension03" }, // 2940526002
		{ &Z_Construct_UFunction_UGameAnalytics_SetEnabledErrorReporting, "SetEnabledErrorReporting" }, // 961141268
		{ &Z_Construct_UFunction_UGameAnalytics_SetEnabledEventSubmission, "SetEnabledEventSubmission" }, // 1184368917
		{ &Z_Construct_UFunction_UGameAnalytics_SetEnabledInfoLog, "SetEnabledInfoLog" }, // 2587768187
		{ &Z_Construct_UFunction_UGameAnalytics_SetEnabledManualSessionHandling, "SetEnabledManualSessionHandling" }, // 3128395058
		{ &Z_Construct_UFunction_UGameAnalytics_SetEnabledVerboseLog, "SetEnabledVerboseLog" }, // 1586938773
		{ &Z_Construct_UFunction_UGameAnalytics_SetWritablePath, "SetWritablePath" }, // 1189385105
		{ &Z_Construct_UFunction_UGameAnalytics_StartSession, "StartSession" }, // 3599358957
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGameAnalytics>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_UGameAnalytics_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_GameAnalytics,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalytics_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UGameAnalytics_Statics::ClassParams = {
	&UGameAnalytics::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	0,
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UGameAnalytics_Statics::Class_MetaDataParams), Z_Construct_UClass_UGameAnalytics_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UGameAnalytics()
{
	if (!Z_Registration_Info_UClass_UGameAnalytics.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGameAnalytics.OuterSingleton, Z_Construct_UClass_UGameAnalytics_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UGameAnalytics.OuterSingleton;
}
template<> GAMEANALYTICS_API UClass* StaticClass<UGameAnalytics>()
{
	return UGameAnalytics::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UGameAnalytics);
UGameAnalytics::~UGameAnalytics() {}
// End Class UGameAnalytics

// Begin Registration
struct Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FGACustomValue::StaticStruct, Z_Construct_UScriptStruct_FGACustomValue_Statics::NewStructOps, TEXT("GACustomValue"), &Z_Registration_Info_UScriptStruct_GACustomValue, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGACustomValue), 3328573524U) },
		{ FGACustomFields::StaticStruct, Z_Construct_UScriptStruct_FGACustomFields_Statics::NewStructOps, TEXT("GACustomFields"), &Z_Registration_Info_UScriptStruct_GACustomFields, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGACustomFields), 1359929604U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGameAnalytics, UGameAnalytics::StaticClass, TEXT("UGameAnalytics"), &Z_Registration_Info_UClass_UGameAnalytics, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGameAnalytics), 2826404972U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_2071791771(TEXT("/Script/GameAnalytics"),
	Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_Statics::ClassInfo),
	Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_ContrarySurvior_ContrarySurvivor_Plugins_GameAnalytics_Source_GameAnalytics_Public_GameAnalytics_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
