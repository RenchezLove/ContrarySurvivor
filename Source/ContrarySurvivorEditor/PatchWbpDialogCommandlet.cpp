// Fill out your copyright notice in the Description page of Project Settings.

#include "PatchWbpDialogCommandlet.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogPatchWbpDialog, Log, All);

namespace
{
	// Roboto движка — как в GenerateWbpCommandlet: шрифт-UObject, гарантированно
	// сериализующийся в пакет (слейтовый шрифт кода в ассет не попадает).
	UObject* LoadRobotoFont()
	{
		return LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	}

	// Одна кнопка диалога: точное имя кнопки, точное имя кубика подписи (сверено с
	// BindWidgetOptional-полями DialogScreenWidget.h) и образец подписи на случай пустой
	// кнопки (виден только в дизайнере — в игре текст ставит код).
	struct FButtonCube
	{
		const TCHAR* ButtonName;
		const TCHAR* TextName;
		const TCHAR* SampleLabel;
	};

	// Рекурсивно собрать текстовые кубики под виджетом (включая его самого): Ринат мог
	// обернуть подпись кнопки в контейнер.
	void CollectTextBlocks(UWidget* Widget, TArray<UTextBlock*>& Out)
	{
		if (!Widget)
		{
			return;
		}
		if (UTextBlock* Block = Cast<UTextBlock>(Widget))
		{
			Out.Add(Block);
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				CollectTextBlocks(Panel->GetChildAt(Index), Out);
			}
		}
	}

	// Обеспечить кубик подписи в одной кнопке (идемпотентно). Подробный лог каждого исхода.
	void PatchOneButton(UWidgetTree* Tree, UObject* Roboto, const FButtonCube& Cube, bool& bChanged)
	{
		const FName TextName(Cube.TextName);

		// Идемпотентность: кубик уже есть — не дублировать.
		if (UWidget* Existing = Tree->FindWidget(TextName))
		{
			UE_LOG(LogPatchWbpDialog, Display, TEXT("'%s' уже есть (%s) — пропуск."),
				Cube.TextName, *Existing->GetClass()->GetName());
			return;
		}

		UWidget* Found = Tree->FindWidget(FName(Cube.ButtonName));
		if (!Found)
		{
			UE_LOG(LogPatchWbpDialog, Warning, TEXT("Кнопка '%s' не найдена — кубик '%s' не добавлен."),
				Cube.ButtonName, Cube.TextName);
			return;
		}
		UButton* Button = Cast<UButton>(Found);
		if (!Button)
		{
			UE_LOG(LogPatchWbpDialog, Warning, TEXT("'%s' — не кнопка (%s), пропуск."),
				Cube.ButtonName, *Found->GetClass()->GetName());
			return;
		}

		UWidget* Content = Button->GetContent();

		// Пустая кнопка: кладём новый текст в стиле кнопок диалога генератора
		// (GenerateWbpCommandlet::BuildDialog — Roboto 15 Regular, белый).
		if (!Content)
		{
			UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
			Label->SetText(FText::FromString(Cube.SampleLabel));
			Label->SetFont(FSlateFontInfo(Roboto, 15, FName(TEXT("Regular"))));
			Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Label->bIsVariable = true;
			Button->SetContent(Label);
			bChanged = true;
			UE_LOG(LogPatchWbpDialog, Display,
				TEXT("'%s': кнопка была пуста — добавлен новый кубик '%s' (стиль генератора)."),
				Cube.ButtonName, Cube.TextName);
			return;
		}

		TArray<UTextBlock*> Texts;
		CollectTextBlocks(Content, Texts);

		// Ровно один текст внутри (обычный случай — статичная подпись из генерации, возможно
		// перестилизованная Ринатом): переименовываем его в имя привязки. Стиль не трогаем
		// вовсе — привязке BindWidgetOptional достаточно имени объекта (см. шапку .h).
		if (Texts.Num() == 1)
		{
			UTextBlock* Label = Texts[0];
			const FString OldName = Label->GetName();
			if (!Label->Rename(*TextName.ToString(), nullptr, REN_Test))
			{
				UE_LOG(LogPatchWbpDialog, Warning,
					TEXT("'%s': текст '%s' нельзя переименовать в '%s' (конфликт имён в пакете) — пропуск."),
					Cube.ButtonName, *OldName, Cube.TextName);
				return;
			}
			Label->Rename(*TextName.ToString(), nullptr, REN_DontCreateRedirectors);
			Label->SetDisplayLabel(TextName.ToString()); // в дизайнере видно реальное имя привязки
			Label->bIsVariable = true;
			bChanged = true;
			UE_LOG(LogPatchWbpDialog, Display,
				TEXT("'%s': текст '%s' переименован в '%s' — стиль Рината сохранён без изменений."),
				Cube.ButtonName, *OldName, Cube.TextName);
			return;
		}

		// Внутри контейнер без текстов: добавляем кубик в конец контейнера (место подправит
		// Ринат в дизайнере — об этом предупреждаем).
		if (Texts.Num() == 0)
		{
			UPanelWidget* Panel = Cast<UPanelWidget>(Content);
			if (!Panel)
			{
				UE_LOG(LogPatchWbpDialog, Warning,
					TEXT("'%s': содержимое '%s' (%s) — не текст и не контейнер, кубик '%s' не добавлен."),
					Cube.ButtonName, *Content->GetName(), *Content->GetClass()->GetName(), Cube.TextName);
				return;
			}
			UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
			Label->SetText(FText::FromString(Cube.SampleLabel));
			Label->SetFont(FSlateFontInfo(Roboto, 15, FName(TEXT("Regular"))));
			Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Label->bIsVariable = true;
			Panel->AddChild(Label);
			bChanged = true;
			UE_LOG(LogPatchWbpDialog, Warning,
				TEXT("'%s': текстов внутри не было — кубик '%s' добавлен в конец контейнера '%s', проверить место в дизайнере."),
				Cube.ButtonName, Cube.TextName, *Panel->GetName());
			return;
		}

		// Несколько текстов: не угадываем, какой из них подпись — пропуск с перечнем.
		FString Names;
		for (const UTextBlock* Block : Texts)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", "));
			Names += Block->GetName();
		}
		UE_LOG(LogPatchWbpDialog, Warning,
			TEXT("'%s': внутри %d текстовых кубиков (%s) — не ясно, какой считать подписью; пропуск."),
			Cube.ButtonName, Texts.Num(), *Names);
	}
}

int32 UPatchWbpDialogCommandlet::Main(const FString& Params)
{
	const TCHAR* PackageName = TEXT("/Game/UI/WBP_Dialog");
	const FString ObjectPath = FString::Printf(TEXT("%s.WBP_Dialog"), PackageName);

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	if (!WBP || !WBP->WidgetTree)
	{
		UE_LOG(LogPatchWbpDialog, Error, TEXT("%s не загрузился — ничего не изменено."), *ObjectPath);
		return 1;
	}
	UE_LOG(LogPatchWbpDialog, Display, TEXT("WBP_Dialog загружен, родитель = %s."),
		WBP->ParentClass ? *WBP->ParentClass->GetPathName() : TEXT("(null)"));

	// Имена кнопок и кубиков — точный контракт DialogScreenWidget.h (BindWidgetOptional).
	// TurnInText в сгенерированном ассете был с самого начала — строка контрольная,
	// ожидаемый исход «уже есть».
	static const FButtonCube Cubes[] =
	{
		{ TEXT("AcceptButton"),  TEXT("AcceptText"),  TEXT("Взяться за дело") },
		{ TEXT("DeclineButton"), TEXT("DeclineText"), TEXT("Отказаться") },
		{ TEXT("CloseButton"),   TEXT("CloseText"),   TEXT("Закрыть") },
		{ TEXT("TurnInButton"),  TEXT("TurnInText"),  TEXT("Сдать (+0)") },
	};

	UObject* Roboto = LoadRobotoFont();
	bool bChanged = false;
	WBP->Modify();
	for (const FButtonCube& Cube : Cubes)
	{
		PatchOneButton(WBP->WidgetTree, Roboto, Cube, bChanged);
	}

	// Итоговая сверка: все четыре кубика должны существовать независимо от пути (переименован /
	// добавлен / был всегда). Нет хотя бы одного — прогон неуспешен.
	int32 MissingCount = 0;
	for (const FButtonCube& Cube : Cubes)
	{
		if (!WBP->WidgetTree->FindWidget(FName(Cube.TextName)))
		{
			UE_LOG(LogPatchWbpDialog, Error, TEXT("ИТОГ: кубик '%s' так и не появился."), Cube.TextName);
			++MissingCount;
		}
	}

	if (!bChanged)
	{
		UE_LOG(LogPatchWbpDialog, Display, TEXT("Менять нечего — ассет не пересохраняется."));
		return MissingCount > 0 ? 1 : 0;
	}

	FKismetEditorUtilities::CompileBlueprint(WBP);
	if (WBP->Status == BS_Error)
	{
		UE_LOG(LogPatchWbpDialog, Error,
			TEXT("WBP_Dialog скомпилировался с ошибками — НЕ сохраняю, файл на диске цел."));
		return 1;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(WBP->GetOutermost(), WBP, *Filename, SaveArgs))
	{
		UE_LOG(LogPatchWbpDialog, Error, TEXT("SavePackage не сохранил %s."), *Filename);
		return 1;
	}

	UE_LOG(LogPatchWbpDialog, Display, TEXT("OK: WBP_Dialog сохранён (%s), недостающих кубиков: %d."),
		*Filename, MissingCount);
	return MissingCount > 0 ? 1 : 0;
}
