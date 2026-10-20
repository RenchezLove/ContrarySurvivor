# Импорт картинок нового вида окна смартфона (художник, 20.10.2026) в /Game/UI/Textures/Pda/:
# текстуры интерфейса — без мип-уровней, группа UI, сжатие для интерфейса.
import os
import unreal

SRC_DIR = r"E:/game-dev-team/assets/pda_restyle_2010"
DEST = "/Game/UI/Textures/Pda"
NAMES = ["T_PdaBtnPlate", "T_PdaBtnPlateActive", "T_PdaMapBtnPlate",
         "T_PdaIcon_Quests", "T_PdaIcon_Map", "T_PdaIcon_Back", "T_PdaIcon_Signal", "T_PdaIcon_Battery"]

tasks = []
for name in NAMES:
    src = os.path.join(SRC_DIR, name + ".png")
    unreal.log("PDA_IMPORT_SRC %s exists=%s" % (src, os.path.isfile(src)))
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

ok = 0
for name in NAMES:
    path = DEST + "/" + name
    tex = unreal.load_asset(path)
    if not tex:
        unreal.log_error("PDA_IMPORT_FAIL %s: not loaded" % path)
        continue
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("srgb", True)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log("PDA_IMPORT_OK %s size=%dx%d compression=%s lod=%s mips=%s saved=%s" % (
        path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y(),
        tex.get_editor_property("compression_settings"), tex.get_editor_property("lod_group"),
        tex.get_editor_property("mip_gen_settings"), saved))
    ok += 1
unreal.log("PDA_IMPORT_DONE %d of %d" % (ok, len(NAMES)))
