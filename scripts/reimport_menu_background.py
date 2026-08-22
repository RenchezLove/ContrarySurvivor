# Замена картинки фона главного меню: импорт PNG поверх /Game/UI/Textures/T_MenuBackground.
import unreal

import os
SRC = r"E:/ContrarySurvior/ContrarySurvivor/Saved/menu_bg_new.png"
unreal.log("SRC=%s exists=%s" % (SRC, os.path.isfile(SRC)))

task = unreal.AssetImportTask()
task.filename = SRC
task.destination_path = "/Game/UI/Textures"
task.destination_name = "T_MenuBackground"
task.replace_existing = True
task.automated = True
task.save = True

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

tex = unreal.load_asset("/Game/UI/Textures/T_MenuBackground")
if tex:
    unreal.log("REIMPORT_OK: %s %dx%d" % (tex.get_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
else:
    unreal.log_error("REIMPORT_FAIL: ассет не загрузился")
