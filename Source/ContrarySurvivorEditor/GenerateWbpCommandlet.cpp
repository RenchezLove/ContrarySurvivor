// Fill out your copyright notice in the Description page of Project Settings.

#include "GenerateWbpCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateWbp, Log, All);

namespace
{
	// Пути спайк-ассета (схема umg-layout-guide.md, раздел WBP_InteractPrompt).
	const TCHAR* PromptPackageName = TEXT("/Game/UI/WBP_InteractPrompt");
	const TCHAR* PromptAssetName = TEXT("WBP_InteractPrompt");
	const TCHAR* PromptParentClassPath = TEXT("/Script/ContrarySurvivor.InteractPromptWidget");
}

int32 UGenerateWbpCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Switches.Contains(TEXT("verify")))
	{
		return VerifyInteractPrompt();
	}
	return GenerateInteractPrompt(Switches.Contains(TEXT("force")));
}

int32 UGenerateWbpCommandlet::GenerateInteractPrompt(bool bForce)
{
	// Защита от перезаписи: ассеты, которые Ринат уже правил, не трогаем без явного -force.
	if (!bForce && FPackageName::DoesPackageExist(PromptPackageName))
	{
		UE_LOG(LogGenerateWbp, Error,
			TEXT("Ассет %s уже существует — генерация отменена (перезапись только с -force)."),
			PromptPackageName);
		return 1;
	}

	UClass* ParentClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, PromptParentClassPath);
	if (!ParentClass)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("Родительский класс %s не найден."), PromptParentClassPath);
		return 1;
	}

	UPackage* Package = CreatePackage(PromptPackageName);
	UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		ParentClass, Package, PromptAssetName, BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	if (!WBP || !WBP->WidgetTree)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("CreateBlueprint не создал Widget Blueprint."));
		return 1;
	}

	// Дерево по схеме: канва на весь экран -> Border-плашка (низ-центр) -> текст PromptText.
	// Точное имя PromptText — контракт BindWidgetOptional (InteractPromptWidget.h); остальные
	// имена и весь стиль (цвет плашки, шрифт, отступы) — свободные, Ринат правит в дизайнере.
	UWidgetTree* Tree = WBP->WidgetTree;
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Tree->RootWidget = Root;

	UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptPlate"));
	Plate->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Plate->SetPadding(FMargin(18.0f, 8.0f));
	Plate->SetHorizontalAlignment(HAlign_Center);
	Plate->SetVerticalAlignment(VAlign_Center);

	UTextBlock* Prompt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
	// Текст-заглушка для превью в дизайнере; в игре его каждый кадр перекрывает код виджета.
	Prompt->SetText(FText::FromString(TEXT("E — подобрать")));
	Prompt->bIsVariable = true;
	Plate->SetContent(Prompt);

	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
	{
		// Низ-центр экрана, плашка обтягивает текст (AutoSize), 140 px от нижней кромки.
		PlateSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		PlateSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		PlateSlot->SetPosition(FVector2D(0.0f, -140.0f));
		PlateSlot->SetAutoSize(true);
	}

	FKismetEditorUtilities::CompileBlueprint(WBP);
	if (WBP->Status == BS_Error)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("Blueprint скомпилировался с ошибками — не сохраняю."));
		return 1;
	}

	WBP->SetFlags(RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(WBP);

	const FString Filename = FPackageName::LongPackageNameToFilename(
		PromptPackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, WBP, *Filename, SaveArgs))
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("SavePackage не сохранил %s."), *Filename);
		return 1;
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("OK: %s создан (родитель %s), файл %s."),
		PromptPackageName, *ParentClass->GetPathName(), *Filename);
	return 0;
}

int32 UGenerateWbpCommandlet::VerifyInteractPrompt()
{
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), PromptPackageName, PromptAssetName);
	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	if (!WBP)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: ассет %s не загрузился."), *ObjectPath);
		return 1;
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY: родительский класс = %s"),
		WBP->ParentClass ? *WBP->ParentClass->GetPathName() : TEXT("(null)"));
	UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY: generated class = %s"),
		WBP->GeneratedClass ? *WBP->GeneratedClass->GetPathName() : TEXT("(null)"));

	UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY: дерево виджетов:"));
	if (WBP->WidgetTree)
	{
		DumpWidgetTree(WBP->WidgetTree->RootWidget, 1);
	}

	const bool bParentOk = WBP->ParentClass
		&& WBP->ParentClass->GetPathName() == PromptParentClassPath;
	const UTextBlock* Prompt = WBP->WidgetTree
		? WBP->WidgetTree->FindWidget<UTextBlock>(TEXT("PromptText"))
		: nullptr;

	if (!bParentOk || !Prompt)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: родитель совпал=%d, PromptText найден=%d."),
			bParentOk ? 1 : 0, Prompt ? 1 : 0);
		return 1;
	}
	UE_LOG(LogGenerateWbp, Display,
		TEXT("VERIFY OK: PromptText (TextBlock) на месте, родитель InteractPromptWidget."));
	return 0;
}

void UGenerateWbpCommandlet::DumpWidgetTree(UWidget* Widget, int32 Depth)
{
	if (!Widget)
	{
		UE_LOG(LogGenerateWbp, Warning, TEXT("VERIFY: дерево пустое (RootWidget = null)."));
		return;
	}
	UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY: %s%s : %s"),
		*FString::ChrN(Depth * 2, TEXT(' ')), *Widget->GetName(), *Widget->GetClass()->GetName());
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			DumpWidgetTree(Panel->GetChildAt(Index), Depth + 1);
		}
	}
}
