// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathScreenWidget.generated.h"

class UTextBlock;
class UButton;
class APlayerCharacter;

/**
 * Экран смерти на UMG (ADR-048, этап 3). Раскладку WBP_Death строит Ринат по схеме
 * docs/contrary-survivor/umg-layout-guide.md (кубики по ТОЧНЫМ именам; BindWidgetOptional —
 * предупреждение, не краш). Статичные строки (заголовок «ВЫ ПОГИБЛИ», строки штрафа по
 * ADR-027/ADR-044, подсказка клавиш) Ринат пишет прямо в WBP; код ставит только живые
 * строки статистики. Возрождение — существующий APlayerCharacter::Respawn (кнопка) и
 * прежние клавиши Enter/Пробел (путь контроллера не тронут).
 */
UCLASS()
class CONTRARYSURVIVOR_API UDeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязка игрока после CreateWidget (статистика последней жизни — его).
	void InitDeath(APlayerCharacter* InPlayer);

	// --- Настройки (Class Defaults WBP_Death; владение переехало из HUD — ADR-048) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "1"))
	FString LifetimePrefix = TEXT("Прожито:  ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "2"))
	FString KillerPrefix = TEXT("Убийца:  ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "3"))
	FString MoneyPrefix = TEXT("Монеты:  ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "4"))
	FString QuestsPrefix = TEXT("Квестов выполнено:  ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "5"))
	FString KillsPrefix = TEXT("Врагов убито:  ");

	// Строка штрафа монет собирается кодом: Prefix + процент + Suffix = «−40% монет — …»
	// (процент живой — из DeathMoneyLossFraction игрока, чтобы текст не расходился с BP).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "6"))
	FString MoneyLossPrefix = TEXT("−");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "7"))
	FString MoneyLossSuffix = TEXT("% монет — часть монет утрачена при гибели.");

protected:
	virtual void NativeOnInitialized() override;

	// Живые строки — каждый кадр (как Canvas DrawDeathScreen; дёшево).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleRespawnClicked();

	// --- Кубики WBP_Death (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LifetimeText;   // «Прожито:  02:31»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KillerText;     // «Убийца:  Волк»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyText;      // «Монеты:  120»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuestsText;     // «Квестов выполнено:  2»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KillsText;      // «Врагов убито:  7»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyLossText;  // «−40% монет — часть монет утрачена при гибели.»

	// Кнопка возрождения; подпись («ВОЗРОДИТЬСЯ») Ринат пишет внутри сам.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RespawnButton;

private:
	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;
};
