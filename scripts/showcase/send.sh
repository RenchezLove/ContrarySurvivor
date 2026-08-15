#!/usr/bin/env bash
# Отправить скрипт в игру и дождаться "QA: SCRIPT done". Использование:
#   send.sh <имя> [таймаут_сек] <<'S'
#   QATeleport 100 200 300
#   wait 1
#   QAShot test
#   S
set -u
PROJ="E:/ContrarySurvior/ContrarySurvivor"
NAME="$1"; TIMEOUT="${2:-120}"
LOG=$(cat "$PROJ/Saved/Showcase/current.log.path")
SEQ=$(date +%H%M%S)
FILE="$PROJ/Saved/Showcase/inbox/${SEQ}_${NAME}.txt"
BEFORE=$(wc -l < "$LOG" 2>/dev/null || echo 0)
cat > "$FILE.tmp" && mv "$FILE.tmp" "$FILE"
T0=$(date +%s)
while true; do
  if tail -n +$((BEFORE+1)) "$LOG" 2>/dev/null | grep -q "QA: SCRIPT done .*${SEQ}_${NAME}"; then break; fi
  if [ $(( $(date +%s) - T0 )) -ge "$TIMEOUT" ]; then echo "TIMEOUT waiting for script done"; break; fi
  sleep 1
done
tail -n +$((BEFORE+1)) "$LOG" | grep -a "QA: SCRIPT\|QA: SHOT\|QA: WOLF\|QA: TELEPORT\|QA: WHERE\|QA: DUMP\|Command not recognized\|LogQA: Warning\|LogQA: Error" | cut -c1-260 | tail -60
