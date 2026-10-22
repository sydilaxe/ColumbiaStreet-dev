#!/bin/sh
# Fetch the curated break and bass samples into the app folder.
#
# They come from TidalCycles' Dirt-Samples library, pinned to one commit. They
# are not committed here: those recordings carry their original rights, so the
# repo only says where to get them. Re-running skips files already present.
#
#   tools/fetch_samples.sh [APP_DIR]   (default dist/ColumbiaStreet)
set -eu

APP=${1:-dist/ColumbiaStreet}
REV=c74fc80f8db8038f6a33648ffef5ac00a07ad402
BASE=https://raw.githubusercontent.com/tidalcycles/Dirt-Samples/$REV

get() { # get SRC DEST
    [ -s "$APP/$2" ] && return 0
    mkdir -p "$(dirname "$APP/$2")"
    if ! curl -fsSL -o "$APP/$2.part" "$BASE/$1" 2>/dev/null; then
        wget -qO "$APP/$2.part" "$BASE/$1" || { rm -f "$APP/$2.part"; echo "failed: $1" >&2; return 1; }
    fi
    mv "$APP/$2.part" "$APP/$2"
}

# Whole loops. The tempo in the name tells Columbia Street how long a 16th is,
# so the slices land on the grid.
get breaks152/000_AMEN.WAV     "breaks/AMEN_153bpm.wav"
get breaks157/000_PLEAD.WAV    "breaks/PLEAD_157bpm.wav"
get breaks165/000_RAWCLN.WAV   "breaks/RAWCLN_165bpm.wav"
get breaks125/015_sdstckbr.wav "breaks/SDSTCK_125bpm.wav"
get breaks125/016_bllstmp.wav  "breaks/BLLSTP_125bpm.wav"

# Pre-chopped: a folder is one bank, its files the slices in name order.
i=1
while [ $i -le 32 ]; do
    n=$(printf "%03d_AMENCUT_%03d" $((i - 1)) $i)
    get "amencutup/$n.wav" "breaks/AMENCUT_149bpm/$n.wav"
    i=$((i + 1))
done
for f in kick1 kick2 snare1 snare2 closedhh openhh ride crash perc1 perc2 hit1 hit2 hit3; do
    get "jungle/jungle4$f.wav" "breaks/JUNGLE/$f.wav"
done

# Bass one-shots for the bass channel's sample voices.
for f in 000_deeep_n_low 001_fat_808_sub 002_fukubass2 003_glide_up_down_sub 004_gliding_808_sub \
    005_jungasubdown 006_junglerevbass 007_junglesine 008_junglesine2 009_junglesine3 \
    010_mega_jungasubdown 011_shiphorn_tekstep_bass 012_short 013_sub_to_open_wah \
    014_sustained_2_octave 015_sustained_3_octave 016_sustained_deep_low 017_sweep_me_low_bass \
    018_synthy_round 019_thin_808_sub; do
    get "jungbass/$f.wav" "bass/$f.wav"
done

echo "samples in $APP/breaks and $APP/bass"
