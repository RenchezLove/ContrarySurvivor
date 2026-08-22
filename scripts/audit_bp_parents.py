# Перечень всех Blueprint/WidgetBlueprint/AnimBlueprint в /Game с родительскими классами (по тегам реестра ассетов).
import unreal
out_path = r"E:/ContrarySurvior/ContrarySurvivor/Saved/audit_bp_parents.txt"
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Game'], True)
assets = ar.get_assets_by_path('/Game', True)
lines = []
for ad in assets:
    cls = str(ad.asset_class_path.asset_name)
    if cls not in ('Blueprint', 'WidgetBlueprint', 'AnimBlueprint'):
        continue
    parent = ad.get_tag_value('ParentClass') or ''
    native = ad.get_tag_value('NativeParentClass') or ''
    lines.append("%s\t%s\t%s\t%s" % (cls, str(ad.package_name), parent, native))
lines.sort()
with open(out_path, 'w', encoding='utf-8') as f:
    f.write("\n".join(lines))
unreal.log("AUDIT_BP_PARENTS: %d blueprints -> %s" % (len(lines), out_path))
