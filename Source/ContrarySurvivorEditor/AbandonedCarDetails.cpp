// Fill out your copyright notice in the Description page of Project Settings.

#include "AbandonedCarDetails.h"

#include "ContrarySurvivor/Actors/AbandonedCar.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AbandonedCarDetails"

TSharedRef<IDetailCustomization> FAbandonedCarDetails::MakeInstance()
{
	return MakeShareable(new FAbandonedCarDetails);
}

void FAbandonedCarDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// «Все именно Наши настройки подними вверх панели Details» — категория машины (вместе
	// с подкатегориями Лут/Цвет/Створки/Стёкла) поднимается приоритетом Important; выше
	// остаётся только Transform (он прижат движком).
	IDetailCategoryBuilder& PartsCategory = DetailBuilder.EditCategory(TEXT("Car"),
		LOCTEXT("CarCategory", "Машина"), ECategoryPriority::Important);

	// Ширина колонки «Есть»: флажок + запас, чтобы колонка «Битая» начиналась ровно под
	// своим заголовком на каждой строке.
	constexpr float PresenceColumnWidth = 56.0f;

	// Заголовок таблицы. Колонка названий — штатная Name-колонка панели, поэтому подпись
	// «Деталь» стоит в NameContent и выравнивается с названиями строк сама.
	FDetailWidgetRow& HeaderRow = PartsCategory.AddCustomRow(LOCTEXT("PartsFilter", "Детали кузова"));
	HeaderRow.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("HeaderPart", "Деталь"))
		.Font(IDetailLayoutBuilder::GetDetailFontBold())
	];
	HeaderRow.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(PresenceColumnWidth)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HeaderPresent", "Есть"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HeaderBroken", "Битая"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	];

	// Строки таблицы. Имена полей — через GET_MEMBER_NAME_CHECKED (AAbandonedCar дружит с
	// этим классом): переименование поля в заголовке машины сломает сборку здесь, а не
	// молча опустошит таблицу. NAME_None в третьей колонке = у накладки битости нет.
	struct FPartRow
	{
		FText Label;
		FName PresenceProp;
		FName BrokenProp;
	};
	const FPartRow Rows[] =
	{
		{ LOCTEXT("DoorFL", "Дверь передняя левая"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorFL), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorFLBroken) },
		{ LOCTEXT("DoorFR", "Дверь передняя правая"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorFR), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorFRBroken) },
		{ LOCTEXT("DoorRL", "Дверь задняя левая"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorRL), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorRLBroken) },
		{ LOCTEXT("DoorRR", "Дверь задняя правая"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorRR), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bDoorRRBroken) },
		{ LOCTEXT("Hood", "Капот"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bHood), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bHoodBroken) },
		{ LOCTEXT("Trunk", "Багажник"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bTrunk), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bTrunkBroken) },
		{ LOCTEXT("WheelFL", "Колесо переднее левое"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelFL), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelFLBroken) },
		{ LOCTEXT("WheelFR", "Колесо переднее правое"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelFR), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelFRBroken) },
		{ LOCTEXT("WheelRL", "Колесо заднее левое"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelRL), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelRLBroken) },
		{ LOCTEXT("WheelRR", "Колесо заднее правое"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelRR), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bWheelRRBroken) },
		{ LOCTEXT("Scratches", "Царапины на кузове"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bScratches), NAME_None },
		{ LOCTEXT("Block", "Блок-подпорка"), GET_MEMBER_NAME_CHECKED(AAbandonedCar, bBlock), NAME_None },
	};

	for (const FPartRow& Row : Rows)
	{
		TSharedRef<IPropertyHandle> Presence = DetailBuilder.GetProperty(Row.PresenceProp);
		DetailBuilder.HideProperty(Presence);

		TSharedPtr<IPropertyHandle> Broken;
		if (!Row.BrokenProp.IsNone())
		{
			Broken = DetailBuilder.GetProperty(Row.BrokenProp);
			DetailBuilder.HideProperty(Broken);
		}

		FDetailWidgetRow& WidgetRow = PartsCategory.AddCustomRow(Row.Label);
		WidgetRow.NameContent()
		[
			SNew(STextBlock)
			.Text(Row.Label)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		TSharedRef<SHorizontalBox> ValueBox = SNew(SHorizontalBox);
		ValueBox->AddSlot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(PresenceColumnWidth)
			[
				// Штатный редактор bool-поля: правка идёт через хэндл со всеми
				// последствиями (транзакция, OnConstruction, «жёлтая стрелка» сброса).
				Presence->CreatePropertyValueWidget()
			]
		];
		if (Broken.IsValid())
		{
			ValueBox->AddSlot()
			.AutoWidth()
			[
				Broken->CreatePropertyValueWidget()
			];
		}
		WidgetRow.ValueContent()
		[
			ValueBox
		];
	}
}

#undef LOCTEXT_NAMESPACE
