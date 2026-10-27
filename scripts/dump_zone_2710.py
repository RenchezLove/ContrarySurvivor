# Снимок настроек зоны главы 2 (чертёж и экземпляр на L_World_C) до и после переименования 27.10.
# Каждое поле читается сначала по новому имени, потом по старому: один и тот же скрипт для обеих версий.
import unreal, sys

out_path = r"E:/ContrarySurvior/ws-pda/logs/zone_dump_2710_%s.txt" % (sys.argv[1] if len(sys.argv) > 1 else "x")
lines = []

def prop(o, names):
    for n in names:
        try:
            v = o.get_editor_property(n)
            return v
        except Exception:
            pass
    return "<нет>"

def fmt(v):
    if isinstance(v, unreal.Vector):
        return "(%.2f, %.2f, %.2f)" % (v.x, v.y, v.z)
    if isinstance(v, unreal.Rotator):
        return "(p%.2f, y%.2f, r%.2f)" % (v.pitch, v.yaw, v.roll)
    if isinstance(v, unreal.LinearColor):
        return "(%.4f, %.4f, %.4f, %.4f)" % (v.r, v.g, v.b, v.a)
    if isinstance(v, float):
        return "%.4f" % v
    if isinstance(v, unreal.Object):
        return v.get_path_name()
    return str(v)

ZONE_PROPS = [
    ("count", ["formation_count", "sample_count"]),
    ("seed", ["random_places_seed"]),
    ("min_dist", ["random_places_min_distance"]),
    ("class", ["formation_class", "sample_class"]),
    ("glow", ["formation_glow_material", "sample_glow_material"]),
    ("snap", ["snap_formations_to_ground", "snap_samples_to_ground"]),
    ("ovr_damage", ["override_damage"]),
    ("dmg_nomask", ["damage_no_mask_per_second"]),
    ("dmg_noair", ["damage_no_air_per_second"]),
    ("ovr_detector", ["override_detector_range"]),
    ("detector", ["detector_range"]),
    ("fog_mat", ["fog_material"]),
    ("fog_mesh", ["fog_layer_mesh"]),
]
FOG_PROPS = ["area_half_size", "density", "fog_color", "layer_count", "edge_softness", "ground_softness", "top_layer_scale"]

def dump_zone(tag, a):
    lines.append("%s class=%s" % (tag, a.get_class().get_name()))
    for label, names in ZONE_PROPS:
        lines.append("%s %s=%s" % (tag, label, fmt(prop(a, names))))
    comps = a.get_components_by_class(unreal.SceneComponent)
    for c in sorted(comps, key=lambda c: c.get_name()):
        cn = c.get_class().get_name()
        if c.component_has_tag("PoisonFogLayer"):
            continue  # слои тумана пересобираются кодом
        s = "%s COMP %s [%s] rel=%s rot=%s scale=%s" % (tag, c.get_name(), cn, fmt(c.get_editor_property("relative_location")),
            fmt(c.get_editor_property("relative_rotation")), fmt(c.get_editor_property("relative_scale3d")))
        if isinstance(c, unreal.BoxComponent):
            s += " extent=%s" % fmt(c.get_editor_property("box_extent"))
        if "Spot" in cn:
            s += " id=%s" % fmt(prop(c, ["formation_id", "sample_id"]))
        if "FogArea" in cn:
            for p in FOG_PROPS:
                s += " %s=%s" % (p, fmt(prop(c, [p])))
        lines.append(s)

bp_class = None
for path in ["/Game/Environment/Chapter2/BP_ToxicAnomalyZone.BP_ToxicAnomalyZone_C", "/Game/Environment/Chapter2/BP_PoisonZone.BP_PoisonZone_C"]:
    bp_class = unreal.load_object(None, path)
    if bp_class:
        lines.append("BP %s" % path)
        break
if bp_class:
    dump_zone("CDO", unreal.get_default_object(bp_class))

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_World_C")
world = unreal.EditorLevelLibrary.get_editor_world()
base = None
for p in ["/Script/ContrarySurvivor.ToxicAnomalyZone", "/Script/ContrarySurvivor.PoisonZone"]:
    base = unreal.load_class(None, p)
    if base:
        break
actors = unreal.GameplayStatics.get_all_actors_of_class(world, base) if base else []
lines.append("LEVEL zones=%d" % len(actors))
for a in actors:
    tag = "INST"
    lines.append("%s label=%s loc=%s rot=%s scale=%s" % (tag, a.get_actor_label(), fmt(a.get_actor_location()), fmt(a.get_actor_rotation()), fmt(a.get_actor_scale3d())))
    dump_zone(tag, a)

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("ZONEDUMP written %s (%d lines)" % (out_path, len(lines)))
