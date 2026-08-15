#!/usr/bin/env bash
# Запуск игры отдельным процессом под съёмку витринных кадров (без редактора).
# Игра сама подхватывает текстовые скрипты из Saved/Showcase/inbox (ключ -ShowcaseWatch,
# реализация — UContraryCheatManager). Размер окна задаётся в ЛОГИЧЕСКИХ единицах экрана
# (масштаб Windows 150%: 1600x720 логических = 2400x1080 физических — проверяется по логу QAShot).
set -u
PROJ="E:/ContrarySurvior/ContrarySurvivor"
ENGINE="E:/UnrealEngine/UE_5.5"
MAP="${1:-/Game/Maps/L_World_C}"
LW="${2:-1600}"; LH="${3:-720}"
STAMP=$(date +%Y%m%d-%H%M%S)
LOG="$PROJ/logs/showcase-$STAMP.log"
mkdir -p "$PROJ/logs" "$PROJ/Saved/Showcase/inbox" "$PROJ/Saved/Showcase/out"
rm -f "$PROJ/Saved/Showcase/inbox/"*.txt "$PROJ/Saved/Showcase/inbox/"*.running 2>/dev/null
echo "$LOG" > "$PROJ/Saved/Showcase/current.log.path"
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*" cmd /c start "" "$ENGINE/Engine/Binaries/Win64/UnrealEditor.exe" "$PROJ/ContrarySurvivor.uproject" "$MAP" -game -windowed -ResX=$LW -ResY=$LH -WinX=0 -WinY=0 -nosplash -nosound -ShowcaseWatch "-abslog=$LOG" "-ExecCmds=r.setres ${LW}x${LH}w"
echo "launched; log=$LOG"
