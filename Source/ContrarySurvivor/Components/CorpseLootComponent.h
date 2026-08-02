// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CorpseLootComponent.generated.h"

class AMasterInventoryItem;

/**
 * Контейнер обыскиваемого лута (Build 1.2.1, ТЗ А1 «Обыск трупов»; Build 1.2.2 — ещё и
 * мешок-пикап, см. APickup).
 *
 * При смерти враг больше не роняет мешок-пикап: деньги и предметы складываются СЮДА
 * (InitLoot из HandleDeath/DropLoot), труп становится «обыскиваемым» — контроллер
 * показывает подсказку «Обыскать [E]» и по E открывает окно UCorpseLootWidget.
 * Частичный обыск штатен: забранное уходит игроку, остаток живёт в компоненте до
 * исчезновения трупа (SetLifeSpan владельца, CorpseLifeSpan врага).
 *
 * Компонент создаётся в КОНСТРУКТОРАХ мастер-классов врагов (AEnemyCharacter,
 * AWolfCharacter) — BP-наследники получают механизм автоматически (ТЗ А1). Build 1.2.2
 * (Ринат: «нужно, чтобы был BP_Picup с механикой, похожей на обыск ящика или трупа»):
 * такой же контейнер несёт и APickup, поэтому мешок на земле открывает ТО ЖЕ окно.
 *
 * Находимость для контроллера — статический реестр «обыскиваемых трупов» (паттерн
 * AEnemyAIController::GetActiveControllers): регистрация в InitLoot (труп появился),
 * снятие в EndPlay (труп исчез по таймеру / конец мира). ПИКАП в реестр НЕ встаёт
 * (bRegisterSearchable=false): его контроллер и так находит перебором APickup, а вторая
 * регистрация дала бы один и тот же мешок двумя интерактивами. Предметы-лут — скрытые
 * акторы-данные; не забранные к EndPlay уничтожаются.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UCorpseLootComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCorpseLootComponent();

	// Положить лут в труп (зовётся ОДИН раз из HandleDeath врага): содержимое ЗАМЕЩАЕТСЯ.
	// По умолчанию регистрирует владельца в реестре обыскиваемых трупов; пикап зовёт с
	// bRegisterSearchable=false (его контроллер находит своим перебором APickup).
	// Items — скрытые акторы-данные (владение переходит сюда).
	void InitLoot(float InMoney, const TArray<AMasterInventoryItem*>& InItems,
		bool bRegisterSearchable = true);

	// ДОБАВИТЬ лут к уже лежащему (Build 1.2.2): пикап наполняется порциями — сначала
	// поля, расставленные дизайнером на карте (BeginPlay), потом рантайм-дроп
	// (InitLoot/InitLootBag пикапа зовутся уже ПОСЛЕ BeginPlay).
	void AddLoot(float InMoney, const TArray<AMasterInventoryItem*>& InItems,
		bool bRegisterSearchable = true);

	// Содержимое изменилось — из контейнера что-то забрали (Build 1.2.2). Слушает владелец:
	// мешок-пикап исчезает, когда его обчистили до конца. Труп не подписан — он лежит до
	// своего таймера, как и раньше.
	FSimpleMulticastDelegate OnLootChanged;

	// Заголовок окна обыска для ЭТОГО контейнера (пусто = окно берёт свой заголовок
	// «Обыск трупа»). Пикап ставит сюда своё название, чтобы над мешком с вещами не
	// висела надпись про труп.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "1",
		DisplayName = "Заголовок окна обыска",
		ToolTip = "Что написано вверху окна обыска для этого контейнера. Пусто = окно возьмёт свой заголовок по умолчанию."))
	FText SearchTitle;

	// Есть ли что забирать (деньги или хоть один живой предмет).
	UFUNCTION(BlueprintPure, Category = "CorpseLoot")
	bool HasLoot() const;

	UFUNCTION(BlueprintPure, Category = "CorpseLoot")
	float GetMoney() const { return Money; }

	// Живые (валидные) предметы трупа — для списка окна обыска.
	TArray<AMasterInventoryItem*> GetLootItems() const;

	// Забрать деньги: возвращает сумму и обнуляет её в трупе.
	float TakeMoney();

	// Забрать предмет: убирает его из списка трупа (true — предмет был здесь).
	// Сам актор НЕ уничтожается — его дальше несёт инвентарь игрока.
	bool TakeItem(AMasterInventoryItem* Item);

	// Реестр обыскиваемых трупов (для UpdateNearbyInteractable контроллера).
	// Слабые ссылки: труп исчезает по таймеру — запись отмирает сама, но чистим в EndPlay.
	static const TArray<TWeakObjectPtr<UCorpseLootComponent>>& GetSearchableCorpses()
	{
		return SearchableCorpses;
	}

protected:
	// Труп исчез (LifeSpan/конец мира): снять с реестра, уничтожить не забранные предметы.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Деньги в трупе (забираются одной кнопкой строки «Деньги»).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CorpseLoot", meta = (AllowPrivateAccess = "true"))
	float Money = 0.0f;

	// Предметы в трупе (скрытые акторы-данные; UPROPERTY — защита от GC до забора).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CorpseLoot", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AMasterInventoryItem>> Items;

	static TArray<TWeakObjectPtr<UCorpseLootComponent>> SearchableCorpses;
};
