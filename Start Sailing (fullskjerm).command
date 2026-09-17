#!/bin/bash
# Dobbeltklikk i Finder for å bygge og starte spillet i fullskjerm (se scripts/run_fullscreen.sh).
cd "$(dirname "$0")"
scripts/run_fullscreen.sh "$@"
status=$?
if [ $status -ne 0 ]; then
	echo
	read -n 1 -s -r -p "Noe gikk galt (kode $status). Trykk en tast for å lukke."
fi
