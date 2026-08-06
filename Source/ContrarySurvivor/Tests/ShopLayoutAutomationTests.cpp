// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест раскладки сетки магазина (Б8, п.5 задания издателя: «растянуть
// сетку на пустующую правую половину»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.ShopLayout; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Живое окно магазина headless не построить (нужен вьюпорт — как и весь Slate-интерфейс
// проекта), но число колонок сетки считает ЧИСТАЯ функция от ширины списка и ширины плитки
// (UShopScreenWidget::ComputeColumnsForWidth), и её проверить можно и нужно: именно она
// решает, жмётся ли каталог узкой колонкой слева или занимает всю доступную ширину.
//
// Числа взяты не с потолка, а с живого ассета WBP_Shop (срез -dumpslots от 06-08):
// панель 1309.3x805.2 при отступе 16 даёт внутреннюю ширину 1277.3; каталог стоит
// растяжкой по долям 0..0.52 с отступами 4 слева и 8 справа -> 652 единицы раскладки,
// минус запас 9 под полосу прокрутки -> 643. Растянутый на всё окно каталог -> 1265
// единиц, минус тот же запас -> 1256.
//
// НЕ покрывается headless: как это выглядит глазами (за Ринатом/живым устройством).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/ShopScreenWidget.h"

static constexpr EAutomationTestFlags ShopLayoutTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Живые значения настроек магазина (Class Defaults WBP_Shop).
static constexpr float ShopTileWidth = 180.0f;
static constexpr float ShopTileSpacing = 8.0f;
static constexpr int32 ShopFallbackColumns = 3;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopLayoutColumnsTest,
	"ContrarySurvivor.ShopLayout.ColumnsFollowListWidth", ShopLayoutTestFlags)

bool FShopLayoutColumnsTest::RunTest(const FString& Parameters)
{
	// 1. Половина окна под каталог: три плитки в ряд, как и было задумано владельцем.
	TestEqual(TEXT("половина окна (643) — три колонки"),
		UShopScreenWidget::ComputeColumnsForWidth(643.0f, ShopTileWidth, ShopTileSpacing, ShopFallbackColumns), 3);

	// 2. Каталог растянут на всё окно (рюкзак пуст) — колонок вдвое больше, пустого места
	//    справа не остаётся. Это и есть требование издателя.
	TestEqual(TEXT("всё окно (1256) — шесть колонок"),
		UShopScreenWidget::ComputeColumnsForWidth(1256.0f, ShopTileWidth, ShopTileSpacing, ShopFallbackColumns), 6);

	// 3. Ширина ещё не измерена (первый кадр окна) — берётся настройка-откат.
	TestEqual(TEXT("ширина не измерена — откат на настройку"),
		UShopScreenWidget::ComputeColumnsForWidth(0.0f, ShopTileWidth, ShopTileSpacing, ShopFallbackColumns),
		ShopFallbackColumns);

	// 4. Ряд считается ТОЧНО: три плитки с двумя зазорами занимают 3*180+2*8 = 556.
	TestEqual(TEXT("ровно под три плитки (556) — три колонки"),
		UShopScreenWidget::ComputeColumnsForWidth(556.0f, ShopTileWidth, ShopTileSpacing, ShopFallbackColumns), 3);
	TestEqual(TEXT("на единицу уже (555) — уже только две"),
		UShopScreenWidget::ComputeColumnsForWidth(555.0f, ShopTileWidth, ShopTileSpacing, ShopFallbackColumns), 2);

	// 5. Совсем узкое окно: колонка всегда хотя бы одна — сетка с нулём колонок обрушила бы
	//    раскладку (деление на число колонок при раскладывании плиток).
	TestEqual(TEXT("узкое окно — минимум одна колонка"),
		UShopScreenWidget::ComputeColumnsForWidth(20.0f, ShopTileWidth, ShopTileSpacing, ShopFallbackColumns), 1);

	// 6. Защита от нулевой ширины плитки (кто-то обнулил настройку): без деления на ноль.
	TestTrue(TEXT("нулевая ширина плитки не роняет расчёт"),
		UShopScreenWidget::ComputeColumnsForWidth(643.0f, 0.0f, 0.0f, ShopFallbackColumns) >= 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
