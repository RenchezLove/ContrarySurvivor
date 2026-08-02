// Copyright ContrarySurvivor. Yandex Mobile Ads bridge module.

using System.IO;
using UnrealBuildTool;

public class YandexAds : ModuleRules
{
	public YandexAds(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Launch — JNI-обвязка движка (AndroidJNI.h), ApplicationCore — GetGameActivityThis().
			PrivateDependencyModuleNames.AddRange(new string[] { "Launch", "ApplicationCore" });

			// UPL дописывает зависимости Gradle, правки манифеста и кладёт Java-прослойку
			// в сборочную папку. Путь относительно движка — тот же приём, что у плагина
			// GameAnalytics в этом проекте (Plugins/GameAnalytics/.../GameAnalytics.Build.cs).
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "YandexAds_UPL.xml"));
		}
	}
}
