// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест положения стика движения. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.TouchStick; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается: центр стика отстоит от НИЖНЕГО края экрана ровно настолько же,
// насколько от ЛЕВОГО (живой осмотр 08-09: «поднять стик вверх, чтобы он был отдалён от
// нижнего края так же сильно, как и от левого»), и замок отступа включён — иначе позиция
// целиком уходит в дизайнер и требование Рината перестаёт держаться кодом.
//
// НЕ покрывается headless: удобство хвата на конкретном телефоне — судит Ринат живьём.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/TouchControlsWidget.h"

static constexpr EAutomationTestFlags TouchStickTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTouchStickCornerOffsetTest,
	"ContrarySurvivor.TouchStick.BottomOffsetEqualsLeftOffset", TouchStickTestFlags)

bool FTouchStickCornerOffsetTest::RunTest(const FString& Parameters)
{
	const UTouchControlsWidget* Defaults = GetDefault<UTouchControlsWidget>();
	if (!TestNotNull(TEXT("Настройки тач-слоя доступны"), Defaults))
	{
		return false;
	}

	TestTrue(TEXT("Замок отступа стика от угла включён (иначе позицию ведёт только дизайнер)"),
		Defaults->bLockStickCorner);

	TestEqual(TEXT("Отступ стика снизу равен отступу слева"),
		Defaults->StickCornerOffsetRef.Y, Defaults->StickCornerOffsetRef.X);

	TestTrue(TEXT("Отступ положительный — стик не выезжает за край экрана"),
		Defaults->StickCornerOffsetRef.X > 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
