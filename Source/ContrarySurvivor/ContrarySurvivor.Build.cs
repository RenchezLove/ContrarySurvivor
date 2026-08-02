// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ContrarySurvivor : ModuleRules
{
	public ContrarySurvivor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UMG — лёгкие виджеты этапа F (ежедневная награда/подсказки); Canvas-HUD не мигрируем.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", });

		// AIModule — для AAIController (AEnemyAIController): MoveToActor, LineOfSightTo, SetFocus.
		// GameplayTasks — транзитивная зависимость AIModule (path following / move tasks).
		// NavigationSystem — UNavigationSystemV1::ProjectPointToNavigation (спавн волков на навмеше).
		// Slate/SlateCore — стили текста UMG-виджетов (FCoreStyle) этапа F.
		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "GameplayTasks", "NavigationSystem", "Slate", "SlateCore" });

		// Этап F3 (ADR-038): аналитика GameAnalytics. Подключаем ТОЛЬКО если плагин лежит в
		// Plugins/ проекта — иначе код собирается с WITH_GAMEANALYTICS=0 и аналитика тихо
		// выключена (рабочая копия без плагина остаётся собираемой).
		string GAPluginPath = Path.Combine(Target.ProjectFile.Directory.FullName,
			"Plugins", "GameAnalytics", "GameAnalytics.uplugin");
		if (File.Exists(GAPluginPath))
		{
			PrivateDependencyModuleNames.Add("GameAnalytics");
			PublicDefinitions.Add("WITH_GAMEANALYTICS=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_GAMEANALYTICS=0");
		}

		// Build 1.2.1: мост к Yandex Mobile Ads (плагин Plugins/YandexAds лежит в репозитории).
		// Вне Android модуль собирается пустышкой, поэтому зависимость безусловная.
		PrivateDependencyModuleNames.Add("YandexAds");

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
