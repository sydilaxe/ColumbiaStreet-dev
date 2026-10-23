#!/bin/sh
# The self-update end to end, without the internet (make update-test; CI runs it).
#
# tools/fake_github.py serves a v9.9.9 release the way GitHub does: the API's JSON, a
# ~1000-character signed redirect, then the zip. build/cs_update installs it over an
# older app folder, as the app does when you press A. First, the same release with a
# damaged zip must fail and leave the folder as it was.
set -eu

T=build/update-test
PORT=${PORT:-8767}
rm -rf $T
mkdir -p $T/rel/App/ColumbiaStreet/breaks/CHOP $T/bad $T/app/breaks

# the release: launch.sh and friends, a binary, a loop, a pre-chopped bank, and a
# columbia.sav that must never reach the user's folder
R=$T/rel/App/ColumbiaStreet
cp dist/ColumbiaStreet/launch.sh dist/ColumbiaStreet/config.json dist/ColumbiaStreet/icon.png \
    dist/ColumbiaStreet/cacert.pem $R/
cp build/cs_update $R/columbia
echo 9.9.9 > $R/VERSION
head -c 300000 /dev/urandom > $R/breaks/LOOP_150bpm.wav
head -c 20000 /dev/urandom > $R/breaks/CHOP/001.wav
echo theirs > $R/columbia.sav
(cd $T/rel && zip -qr ColumbiaStreet-miyoo.zip App)

# the Miyoo's folder: an older version, the user's song and break, and one sample
# that's already the same as the release's (dated 2001, so we can tell it's left alone)
A=$T/app
cp dist/ColumbiaStreet/cacert.pem $A/
echo old > $A/columbia
echo old > $A/launch.sh
echo 0.1.0 > $A/VERSION
echo mine > $A/columbia.sav
head -c 1000 /dev/urandom > $A/breaks/MINE_120bpm.wav
cp $R/breaks/LOOP_150bpm.wav $A/breaks/
touch -t 200101010000 $A/breaks/LOOP_150bpm.wav

# the same release with its zipped binary damaged (the CRC check must catch it)
cp $T/rel/ColumbiaStreet-miyoo.zip $T/bad/
off=$(python3 -c '
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); i = z.getinfo("App/ColumbiaStreet/columbia")
print(i.header_offset + 30 + len(i.filename) + len(i.extra) + i.compress_size // 2)' $T/bad/ColumbiaStreet-miyoo.zip)
printf 'XXXXXXXX' | dd of=$T/bad/ColumbiaStreet-miyoo.zip bs=1 seek="$off" conv=notrunc 2>/dev/null

python3 tools/fake_github.py $T/rel --port $PORT --tag v9.9.9 &
good=$!
python3 tools/fake_github.py $T/bad --port $((PORT + 1)) --tag v9.9.9 &
bad=$!
trap 'kill $good $bad 2>/dev/null || true' EXIT
for i in $(seq 1 50); do
    curl -s -o /dev/null http://127.0.0.1:$PORT/latest.json && curl -s -o /dev/null http://127.0.0.1:$((PORT + 1))/latest.json && break
    sleep 0.1
done

fail() { echo "update test: $*" >&2; exit 1; }

cp -r $A $T/before
if CS_UPDATE_API=http://127.0.0.1:$((PORT + 1))/latest.json build/cs_update $A; then
    fail "a damaged zip was installed"
fi
diff -r $T/before $A >/dev/null || fail "a failed update changed the app folder"
echo "update test: a damaged zip is refused and changes nothing"

CS_UPDATE_API=http://127.0.0.1:$PORT/latest.json build/cs_update $A
cmp -s $A/columbia $R/columbia || fail "the binary wasn't replaced"
test -x $A/columbia || fail "the binary isn't executable"
cmp -s $A/launch.sh $R/launch.sh || fail "launch.sh wasn't replaced"
test "$(cat $A/VERSION)" = 9.9.9 || fail "VERSION isn't 9.9.9"
cmp -s $A/breaks/CHOP/001.wav $R/breaks/CHOP/001.wav || fail "the pre-chopped bank's folder wasn't made"
test "$(cat $A/columbia.sav)" = mine || fail "the user's song was overwritten"
test -f $A/breaks/MINE_120bpm.wav || fail "the user's break is gone"
test "$(date -r $A/breaks/LOOP_150bpm.wav +%Y)" = 2001 || fail "an unchanged sample was rewritten"
test -z "$(find $A -name '*.new')" || fail ".new files were left behind"
echo "update test: installed v9.9.9 over v0.1.0, kept the song and the user's break, skipped the unchanged sample"
