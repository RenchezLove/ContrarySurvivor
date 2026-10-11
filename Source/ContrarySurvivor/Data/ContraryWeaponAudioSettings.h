// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "ContraryWeaponAudioSettings.generated.h"

class USoundBase;

/**
 * Звуки огнестрела, общие для всех стволов (11.10.2026, слова Рината: «необходимо сообщать
 * игроку о малом количестве патронов в магазине - звуком. Когда патронов в магазине нет совсем
 * - нажатие на кнопку выстрела должно воспроизводить звук щелчка… Ударник бьётся вхолостую»).
 * Страница в Project Settings → Game → «Contrary Survivor: звуки оружия»; значения пишутся в
 * Config/DefaultGame.ini.
 *
 * ПУСТАЯ ссылка или файл, которого нет в проекте, = тишина без ошибок. У отдельного ствола
 * звук можно заменить своим (поля «Звук: мало патронов» и «Звук: пустой магазин» на оружии).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Contrary Survivor: звуки оружия"))
class CONTRARYSURVIVOR_API UContraryWeaponAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	// Играет ОДИН раз — в момент, когда после выстрела в магазине осталась заданная доля.
	UPROPERTY(config, EditAnywhere, Category = "Мало патронов", meta = (
		DisplayName = "Звук: в магазине мало патронов",
		ToolTip = "Играет один раз, когда после выстрела в магазине осталась четверть патронов (долю задаёт поле ниже). Пусто = тишина."))
	TSoftObjectPtr<USoundBase> LowAmmoSound;

	// Какая доля магазина считается «мало» (0.25 = четверть). Порог в патронах — доля от
	// вместимости с округлением вниз, но не меньше одного патрона.
	UPROPERTY(config, EditAnywhere, Category = "Мало патронов", meta = (ClampMin = "0.0", ClampMax = "1.0",
		DisplayName = "Доля магазина, при которой звучит предупреждение",
		ToolTip = "0.25 = звук, когда осталась четверть магазина. Порог не бывает меньше одного патрона. 0 = звук выключен."))
	float LowAmmoClipFraction = 0.25f;

	UPROPERTY(config, EditAnywhere, Category = "Мало патронов", meta = (ClampMin = "0.0",
		DisplayName = "Громкость предупреждения"))
	float LowAmmoVolume = 1.0f;

	// Играет при каждом нажатии выстрела с пустым магазином.
	UPROPERTY(config, EditAnywhere, Category = "Пустой магазин", meta = (
		DisplayName = "Звук: выстрел с пустым магазином (щелчок)",
		ToolTip = "Сухой щелчок ударника при нажатии выстрела без патронов в магазине. Пусто = тишина."))
	TSoftObjectPtr<USoundBase> DryFireSound;

	// Кнопка выстрела при удержании срабатывает каждый кадр — щелчок звучит не чаще этого.
	UPROPERTY(config, EditAnywhere, Category = "Пустой магазин", meta = (ClampMin = "0.05",
		DisplayName = "Пауза между щелчками, сек",
		ToolTip = "Не чаще одного щелчка за это время, даже если кнопка выстрела зажата. 0.25 = не чаще четырёх раз в секунду."))
	float DryFireMinInterval = 0.25f;

	UPROPERTY(config, EditAnywhere, Category = "Пустой магазин", meta = (ClampMin = "0.0",
		DisplayName = "Громкость щелчка"))
	float DryFireVolume = 1.0f;

	static const UContraryWeaponAudioSettings* Get() { return GetDefault<UContraryWeaponAudioSettings>(); }
};
