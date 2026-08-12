// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using Microsoft.Extensions.Logging; // Logger.LogInformation/LogWarning — методы расширения

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
		// RenderCore — счётчики GGameThreadTime/GRenderThreadTime (RenderTimer.h), RHI — GGPUFrameTime
		// (RHIGlobals.h). Нужны замеру времён кадра в ContrarySurvivorStatics; без них падает компоновка.
		// EngineSettings — UGameMapsSettings: карта по умолчанию и правило «какой режим игры
		// достаётся уровню по началу его имени». Нужен автотесту переезда запуска (ADR-067 п.5):
		// строку настройки легко написать с опечаткой, и тогда загрузочный уровень тихо получил
		// бы обычный режим игры вместе с персонажем и лесом. Engine тянет этот модуль публично,
		// но для СВЯЗЫВАНИЯ его всё равно нужно назвать здесь явно (иначе LNK2019).
		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "GameplayTasks", "NavigationSystem", "Slate", "SlateCore", "RenderCore", "RHI", "EngineSettings" });

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

		AddAnalyticsKeyDefinitions();

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}

	/// <summary>
	/// Б4 (задание издателя, ADR-059): ключи GameAnalytics вшиваются в двоичный файл НА ЭТАПЕ
	/// КОМПИЛЯЦИИ. До этого AnalyticsSubsystem искал файлы ключей на диске уже в игре, а в
	/// собранном пакете (телефон) такой папки нет — аналитика молча выключалась, ни одно
	/// событие не уходило (журнал живого телефона, ревизия ADR-058 п.5).
	///
	/// Сами значения ключей в репозиторий НЕ попадают (ADR-013, гейм-репо публичный): они
	/// читаются здесь, из локальной папки ВНЕ репозитория, и уходят прямо в командную строку
	/// компилятора. Папку задаёт переменная окружения CONTRARY_ANALYTICS_KEYS_DIR, по
	/// умолчанию — E:/game-dev-team/keys (рабочая машина). Нет папки или файлов — модуль
	/// собирается без вшитых ключей, и аналитика в игре откатывается на прежний поиск файлов
	/// по диску (у разработчика локально ничего не ломается).
	/// </summary>
	private void AddAnalyticsKeyDefinitions()
	{
		string KeysFolder = System.Environment.GetEnvironmentVariable("CONTRARY_ANALYTICS_KEYS_DIR");
		if (string.IsNullOrWhiteSpace(KeysFolder))
		{
			KeysFolder = "E:/game-dev-team/keys";
		}

		string GameKeyFile = Path.Combine(KeysFolder, "GameKey.txt");
		string SecretKeyFile = Path.Combine(KeysFolder, "SecretKey.txt");
		string GameKey = ReadAnalyticsKeyFile(GameKeyFile);
		string SecretKey = ReadAnalyticsKeyFile(SecretKeyFile);

		if (GameKey.Length == 0 || SecretKey.Length == 0)
		{
			PrivateDefinitions.Add("CONTRARY_GA_KEYS_COMPILED_IN=0");
			Logger.LogInformation(
				"ContrarySurvivor: ключи аналитики НЕ вшиты — нет пригодных GameKey.txt/SecretKey.txt в '{Folder}' (папку задаёт CONTRARY_ANALYTICS_KEYS_DIR). Аналитика будет искать файлы в рантайме.",
				KeysFolder);
			return;
		}

		// Правка файла ключей должна приводить к пересборке. Регистрируем зависимость ТОЛЬКО
		// для существующих файлов: несуществующий файл в этом списке заставляет UBT считать
		// makefile устаревшим при каждом запуске (TargetMakefile.cs: "{File} has been deleted").
		ExternalDependencies.Add(GameKeyFile);
		ExternalDependencies.Add(SecretKeyFile);

		// PrivateDefinitions, а не Public: значения видит только этот модуль, в командные
		// строки остальных модулей и целей ключи не расходятся.
		PrivateDefinitions.Add("CONTRARY_GA_GAME_KEY=\"" + GameKey + "\"");
		PrivateDefinitions.Add("CONTRARY_GA_SECRET_KEY=\"" + SecretKey + "\"");
		PrivateDefinitions.Add("CONTRARY_GA_KEYS_COMPILED_IN=1");

		// В журнал сборки — только длины ключей, сами значения не печатаем никогда.
		Logger.LogInformation(
			"ContrarySurvivor: ключи аналитики вшиты в сборку из '{Folder}' (длины {GameLen}/{SecretLen} символов).",
			KeysFolder, GameKey.Length, SecretKey.Length);
	}

	/// <summary>
	/// Читает файл ключа. Возвращает пустую строку, если файла нет, он пуст или содержит
	/// символы, недопустимые внутри строкового литерала C++ (кавычка, обратный слэш, перевод
	/// строки) — такой ключ вшивать нельзя, он сломал бы компиляцию.
	/// </summary>
	private string ReadAnalyticsKeyFile(string FilePath)
	{
		if (!File.Exists(FilePath))
		{
			return string.Empty;
		}

		string Key;
		try
		{
			Key = File.ReadAllText(FilePath).Trim();
		}
		catch (System.Exception Ex)
		{
			Logger.LogWarning("ContrarySurvivor: не прочитать файл ключа '{File}': {Message}", FilePath, Ex.Message);
			return string.Empty;
		}

		foreach (char C in Key)
		{
			bool bAllowed = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z')
				|| (C >= '0' && C <= '9') || C == '-' || C == '_';
			if (!bAllowed)
			{
				Logger.LogWarning(
					"ContrarySurvivor: файл ключа '{File}' содержит недопустимый символ — ключ не вшивается.",
					FilePath);
				return string.Empty;
			}
		}
		return Key;
	}
}
