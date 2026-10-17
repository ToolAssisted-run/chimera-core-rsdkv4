#!/bin/sh
# run-gate.sh - the Retro Engine v4 core's tests: the native reference and
# core.wbx are the same machine, on the core's own test game
# (tests/make-test-game.py, played with Skip Start Menu, as it has no menus)
# and, when they are given, on Sonic 1 and Sonic 2 themselves.
#
# Every test says what it compared. The tests:
#   equivalence  run-native and run-wbx: every frame's picture, sound, length
#                and lag, the frame count and every memory domain
#   rerecord     the sandbox saved and loaded before every frame = without
#   session      saved at a frame, a new host, loaded, finished = without
#   turbo        the first half not drawn: the rest the same, and the first
#                half's pictures really not drawn
#   input        the movie's presses reach the game's script: three presses of
#                A counted, Right and Left move the object 2 pixels a frame;
#                the frame that loads the stage is the only lag frame
#   saves        the game's save written (Start) and read back (C) in the same
#                run, in both builds, and across a session started after the
#                write
#   pokes        a property written before a frame lands in the game: the
#                object's position, a global variable by its name
#   settings     randomSeed is what the game's random numbers follow (the same
#                seed the same numbers, another seed others); screenWidth is
#                the picture's width; renderScale 2 draws it twice the size;
#                the game setting picks the data file (Sonic2.rsdk)
#   refusals     no data file, a file that is not one, one with nothing in it,
#                a screen width or a render scale out of range: each says why
#   clock        the guest has no time() of its own: the one it calls is the
#                core's (the setting)
#   teeth        the equivalence comparison sees a one-frame difference
#   firmware     waterbox.config's firmware is the files the core opens, one a
#                game
#   game         (with -1 and -2) the equivalence, rerecord and session tests
#                on Sonic 1's and Sonic 2's data files, which are never in the
#                repository: 4100 frames from power-on through the remaster's
#                menus (Start Game, No Save, Sonic) into the first zone
#
# usage: run-gate.sh [-m <miniBox dir>] [-1 <Sonic 1 Data.rsdk>] [-2 <Sonic 2 Data.rsdk>]
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
mb="${MINIBOX_DIR:-$HOME/chimera/extern/chimera-common-minibox}"
sonic1=""
sonic2=""
while getopts "m:1:2:" opt; do
	case "$opt" in
		m) mb="$OPTARG" ;;
		1) sonic1="$OPTARG" ;;
		2) sonic2="$OPTARG" ;;
		*) exit 2 ;;
	esac
done
mb="$(cd "$mb" && pwd)"

echo "== build (miniBox $mb)"
mkdir -p "$root/build"
# the OpenGL both builds draw with: Mesa, built once into build/mesa
bash "$here/setup-mesa.sh" -m "$mb" > "$root/build/gate-mesa.log" 2>&1 && bash "$here/setup-mesa.sh" -n >> "$root/build/gate-mesa.log" 2>&1 || {
	tail -20 "$root/build/gate-mesa.log"; echo "the Mesa build failed"; exit 1; }
make -C "$here" -f native.mk MB="$mb" -j"$(nproc)" > "$root/build/gate-native.log" 2>&1 || {
	tail -20 "$root/build/gate-native.log"; echo "native build failed"; exit 1; }
make -C "$here" -f guest.mk MB="$mb" -j"$(nproc)" > "$root/build/gate-guest.log" 2>&1 || {
	tail -20 "$root/build/gate-guest.log"; echo "guest build failed"; exit 1; }
native="$root/build/native/run-native"
wbx="$root/build/native/run-wbx"
core="$root/build/guest/core.wbx"
movie="$here/tests/test.movie"
frames=100

work="$root/build/gate"
rm -rf "$work"
mkdir -p "$work"
fails=0
pass() { echo "PASS $*"; }
fail() { echo "FAIL $*"; fails=$((fails + 1)); }

# the digests only (miniBox's own log lines go to the same stdout)
digests() { grep -E '^[A-Za-z]+(\[[^]]*\])?=' "$1" || true; }
same() { # same <test> <a> <b> <what>
	digests "$2" > "$2.d"
	digests "$3" > "$3.d"
	if cmp -s "$2.d" "$3.d"; then
		pass "$1: $4"
	else
		fail "$1: $4"; diff "$2.d" "$3.d" | head -20
	fi
}
value() { digests "$1" | sed -n "s/^$2=//p"; }
# a traced property's value at a frame (a trace line: frame rate read values...)
at() { awk -v f="$2" -v col="$3" '$1 == f { print $(3 + col) }' "$1"; }

# the test game is the Sonic 1 firmware's file, played without the start
# menu (it has none)
no_menu='{"skipStartMenu": true}'
mkgame() { # mkgame <dir> [<settings JSON>]
	mkdir -p "$work/$1"
	python3 "$here/tests/make-test-game.py" "$work/$1/Sonic1.rsdk"
	printf '%s' "${2:-$no_menu}" > "$work/$1/settings"
}

mkgame synth
echo "== the test game ($frames frames, $movie)"
"$native" "$work/synth" --frames $frames --movie "$movie" > "$work/n.txt"
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" > "$work/w.txt" 2> "$work/w.err"
same equivalence "$work/n.txt" "$work/w.txt" "native = sandbox, $frames frames (lag $(value "$work/n.txt" lagFrames))"
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --rerecord > "$work/r.txt" 2> "$work/r.err"
same rerecord "$work/r.txt" "$work/w.txt" "save+load before every frame = straight ($(sed -n 's/stateBytes=//p' "$work/r.err") state bytes)"
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --session-at 75 > "$work/s.txt" 2> "$work/s.err"
same session "$work/s.txt" "$work/w.txt" "saved at frame 75 (after the game's save was written), loaded into a new host = straight"

"$native" "$work/synth" --frames $frames --movie "$movie" --turbo > "$work/t.txt"
digests "$work/t.txt" | grep -v '^videoHash=' > "$work/t.d"
digests "$work/n.txt" | grep -v '^videoHash=' > "$work/n.d"
if cmp -s "$work/t.d" "$work/n.d" && [ "$(value "$work/t.txt" videoHash)" != "$(value "$work/n.txt" videoHash)" ]; then
	pass "turbo: first half undrawn - the second half's pictures, the sound, the frames and memory the same; the first half's pictures not"
else
	fail "turbo"; diff "$work/t.d" "$work/n.d" | head
fi

echo "== input and saves"
props="test.presses,test.random,test.saved,Object.X[32]"
"$native" "$work/synth" --frames $frames --movie "$movie" --trace "$work/in.n" --trace-props "$props" > /dev/null
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --trace "$work/in.w" --trace-props "$props" > /dev/null 2>&1
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --session-at 75 --trace "$work/in.s" --trace-props "$props" > /dev/null 2>&1
# the object starts at x 100: Right 20 frames (+40), Left 10 frames (-20)
if [ "$(at "$work/in.n" 79 1)" = 3 ] && [ "$(at "$work/in.n" 99 1)" = 4 ] && [ "$(at "$work/in.n" 49 4)" = $((140 << 16)) ] &&
   [ "$(at "$work/in.n" 99 4)" = $((120 << 16)) ] && [ "$(value "$work/n.txt" lagFrames)" = 1 ] && [ "$(at "$work/in.n" 0 0)" = 0 ]; then
	pass "input: A counted 3 times by frame 79 and 4 by 99; Right then Left move the object to x 140, then 120; one lag frame, the stage's load"
else
	fail "input: presses $(at "$work/in.n" 79 1)/$(at "$work/in.n" 99 1), x $(at "$work/in.n" 49 4)/$(at "$work/in.n" 99 4), lag $(value "$work/n.txt" lagFrames)"
fi
if [ "$(at "$work/in.n" 89 3)" = -1 ] && [ "$(at "$work/in.n" 90 3)" = 3 ] && cmp -s "$work/in.n" "$work/in.w"; then
	pass "saves: the count written at frame 70 (3) is read back at frame 90, in both builds"
else
	fail "saves: before the read $(at "$work/in.n" 89 3), after $(at "$work/in.n" 90 3)"
fi
if [ "$(at "$work/in.s" 90 3)" = 3 ]; then
	pass "saves: a session started at frame 75 reads back the save written at 70"
else
	fail "saves: after a session, read back $(at "$work/in.s" 90 3)"
fi

echo "== pokes"
"$native" "$work/synth" --frames 30 --poke "20:Object.X[32]=13107200" --poke "25:test.presses=40" --trace "$work/pk.n" --trace-props "Object.X[32],test.presses" > /dev/null
"$wbx" "$core" "$work/synth" --frames 30 --poke "20:Object.X[32]=13107200" --poke "25:test.presses=40" --trace "$work/pk.w" --trace-props "Object.X[32],test.presses" > /dev/null 2>&1
if [ "$(at "$work/pk.n" 19 1)" = $((100 << 16)) ] && [ "$(at "$work/pk.n" 20 1)" = 13107200 ] && [ "$(at "$work/pk.n" 25 2)" = 40 ] && cmp -s "$work/pk.n" "$work/pk.w"; then
	pass "pokes: Object.X[32] and test.presses (a global variable by its name) written before a frame are what the game has after it, in both builds"
else
	fail "pokes: x $(at "$work/pk.n" 20 1), presses $(at "$work/pk.n" 25 2)"
fi

echo "== settings"
seeded() { # seeded <dir> <seed>
	mkgame "$1" "{\"skipStartMenu\": true, \"randomSeed\": $2}"
	"$native" "$work/$1" --frames $frames --movie "$movie" --trace "$work/$1/t" --trace-props test.random > /dev/null
	"$wbx" "$core" "$work/$1" --frames $frames --movie "$movie" --trace "$work/$1/tw" --trace-props test.random > /dev/null 2>&1
	awk 'NR > 1 { print $4 }' "$work/$1/t" | uniq | tr '\n' ' '
}
s0="$(seeded seed0 0)"
s0b="$(seeded seed0b 0)"
s7="$(seeded seed7 7)"
if [ "$s0" = "$s0b" ] && [ "$s0" != "$s7" ] && cmp -s "$work/seed7/t" "$work/seed7/tw"; then
	pass "settings: randomSeed 0 draws ${s0% } twice, 7 draws ${s7% }; both builds the same"
else
	fail "settings: randomSeed 0 ${s0}/${s0b}, 7 ${s7}"
fi
size() { python3 -c "import sys; d = open(sys.argv[1], 'rb').read(18); print('%dx%d' % (d[12] | d[13] << 8, d[14] | d[15] << 8))" "$1" 2>/dev/null || echo none; }
mkgame narrow '{"skipStartMenu": true, "screenWidth": 320}'
"$native" "$work/narrow" --frames 10 --screenshot 9:"$work/narrow/s.tga" > "$work/narrow/n" 2>&1 || true
"$wbx" "$core" "$work/narrow" --frames 10 > "$work/narrow/w" 2>&1 || true
if [ "$(size "$work/narrow/s.tga")" = 320x240 ]; then
	same settings "$work/narrow/w" "$work/narrow/n" "screenWidth 320 draws a 320x240 picture; native = sandbox"
else
	fail "settings: screenWidth 320 drew $(size "$work/narrow/s.tga")"
fi
mkgame double '{"skipStartMenu": true, "renderScale": 2}'
"$native" "$work/double" --frames 10 --screenshot 9:"$work/double/s.tga" > "$work/double/n" 2>&1 || true
"$wbx" "$core" "$work/double" --frames 10 > "$work/double/w" 2>&1 || true
if [ "$(size "$work/double/s.tga")" = 848x480 ]; then
	same settings "$work/double/w" "$work/double/n" "renderScale 2 draws an 848x480 picture; native = sandbox"
else
	fail "settings: renderScale 2 drew $(size "$work/double/s.tga")"
fi
mkdir -p "$work/second"
python3 "$here/tests/make-test-game.py" "$work/second/Sonic2.rsdk"
printf '{"skipStartMenu": true, "game": "sonic2"}' > "$work/second/settings"
"$native" "$work/second" --frames $frames --movie "$movie" > "$work/second/n" 2>&1 || true
if digests "$work/second/n" | cmp -s - "$work/n.txt.d"; then
	pass "settings: the game setting sonic2 plays Sonic2.rsdk (the same test game, the same run)"
else
	fail "settings: game sonic2"; tail -2 "$work/second/n"
fi

echo "== refusals"
refuse() { # refuse <dir> <expected text> <what>
	out="$("$native" "$work/$1" --frames 1 2>&1 || true)"
	outw="$("$wbx" "$core" "$work/$1" --frames 1 2>&1 || true)"
	if echo "$out" | grep -q "loadError=.*$2" && echo "$outw" | grep -q "loadError=.*$2"; then
		pass "refusals: $3 - $(echo "$out" | sed -n 's/^loadError=//p')"
	else
		fail "refusals: $3"; echo "$out" | tail -2
	fi
}
mkdir -p "$work/none" "$work/junk" "$work/empty"
head -c 100000 /dev/zero > "$work/junk/Sonic1.rsdk"
printf 'RSDKvB\0\0' > "$work/empty/Sonic1.rsdk"
mkgame wide '{"screenWidth": 640}'
mkgame big '{"renderScale": 3}'
refuse none "Sonic1.rsdk) is not there" "no data file"
refuse junk "not a Retro Engine v4 data file" "a file of zeros"
refuse empty "has no Data/Game/GameConfig.bin" "a data file with no files in it"
refuse wide "goes from 320 to 424" "screenWidth 640"
refuse big "goes from 1 to 2" "renderScale 3"

echo "== clock"
if nm "$core" | grep -qE ' (T|W|t) (time|__real_time)$'; then
	fail "clock: the guest links a time() of the C library's"
else
	pass "clock: time() is only the core's (the randomSeed setting); SDL's counters are the frame count"
fi

echo "== teeth"
# frame 30 is in the Right stretch: Left instead
awk '!/^#/ { n++ } n == 31 && !/^#/ { print "L"; next } { print }' "$movie" > "$work/teeth.movie"
"$native" "$work/synth" --frames $frames --movie "$work/teeth.movie" > "$work/teeth.txt"
if digests "$work/teeth.txt" | cmp -s - "$work/n.txt.d"; then
	fail "teeth: a movie one frame different digests the same"
else
	pass "teeth: a movie one frame different digests differently"
fi

echo "== firmware"
fw="$(python3 -c "import json, sys; print(' '.join(f['id'] + '/' + f['requiredWhen']['is'] for f in json.load(open(sys.argv[1]))['firmware']))" "$here/waterbox.config")"
if [ "$fw" = "Sonic1.rsdk/sonic1 Sonic2.rsdk/sonic2" ] && grep -q 'st->game == 2 ? "Sonic2.rsdk" : "Sonic1.rsdk"' "$here/rsdk-files.c"; then
	pass "firmware: waterbox.config's two firmware files, one a game, are the names the core opens"
else
	fail "firmware: waterbox.config declares '$fw'"
fi

# the remaster's way in: Start on the title, A on Start Game, A on No Save,
# A on Sonic; Start twice on the game's own title; then right and two jumps
route="--press 1300:S:5 --press 1600:A:5 --press 1900:A:5 --press 2200:A:5 --press 2900:S:5 --press 3200:S:5 \
	--press 3500:R:600 --press 3600:A:10 --press 3800:A:10"
real_game() { # real_game <name> <setting> <file>
	[ -n "$3" ] || { echo "SKIP $1: no data file given; it is never in the repository"; return; }
	echo "== $1 ($3)"
	d="$work/$2"
	mkdir -p "$d"
	if [ "$2" = sonic2 ]; then cp "$3" "$d/Sonic2.rsdk"; else cp "$3" "$d/Sonic1.rsdk"; fi
	printf '{"game": "%s"}' "$2" > "$d/settings"
	gn=4100
	gat=$((gn * 9 / 10))
	"$native" "$d" --frames $gn $route --trace "$d/t" --trace-props "Stage.List,Stage.Position" > "$d/n"
	"$wbx" "$core" "$d" --frames $gn $route > "$d/w" 2>/dev/null
	stage="$(tail -1 "$d/t" | awk '{ print $4 "/" $5 }')"
	same "$1" "$d/w" "$d/n" "native = sandbox, $gn frames (stage list/position at the end: $stage)"
	"$wbx" "$core" "$d" --frames $gn $route --rerecord > "$d/r" 2>/dev/null
	same "$1" "$d/r" "$d/w" "rerecord = straight"
	"$wbx" "$core" "$d" --frames $gn $route --session-at $gat > "$d/s" 2>/dev/null
	same "$1" "$d/s" "$d/w" "session at $gat = straight"
}
real_game "Sonic 1" sonic1 "$sonic1"
real_game "Sonic 2" sonic2 "$sonic2"

echo
if [ $fails -eq 0 ]; then
	echo "gate: all tests passed"
else
	echo "gate: $fails test(s) failed"
	exit 1
fi
