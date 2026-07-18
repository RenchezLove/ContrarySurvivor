// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

// Editor-модуль (ADR-048): программная генерация WBP-ассетов (Widget Blueprint) для Рината —
// коммандлет строит дерево виджетов кодом, Ринат дальше правит стиль мышкой в дизайнере.
// В игру (Runtime/Android-пак) модуль НЕ попадает: Type=Editor в .uproject.
public class ContrarySurvivorEditor : ModuleRules
{
	public ContrarySurvivorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// UMG — рантайм-виджеты (UWidgetTree, UCanvasPanel, UBorder, UTextBlock);
		// UMGEditor — UWidgetBlueprint; UnrealEd — FKismetEditorUtilities (создание/компиляция BP);
		// AssetRegistry — регистрация нового ассета; ContrarySurvivor — родительские C++-классы виджетов.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UMG", "UMGEditor", "UnrealEd", "AssetRegistry", "Slate", "SlateCore", "ContrarySurvivor"
		});
	}
}
