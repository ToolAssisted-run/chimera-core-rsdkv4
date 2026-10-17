#!/usr/bin/env python3
"""A Retro Engine v4 game made for the core's tests, written out from
scratch: no part of any Sega game is in it.

    make-test-game.py <out Data.rsdk>

What is in the data file (a pack in the engine's v4 format, "RSDKvB": each
file named by the MD5 of its lower-case path, and stored unencrypted, which
the format allows; Reader.cpp):

  Data/Game/GameConfig.bin  the game: three global variables (test.presses,
                            test.random, test.saved), one global sound effect, one
                            stage in the presentation list
  Data/SoundFX/Test/Beep.wav  a short square wave
  Data/Music/Tune.ogg       tests/tune.ogg, two seconds of two tones
  Data/Stages/Test/StageConfig.bin, Act1.bin  the stage: one object
  Data/Scripts/Test/Box.txt the object's script, in the engine's script
                            language, which the engine compiles at load

The object, Box, is the test: it starts the stage's timer and the music, moves with Left and
Right, and on each press of A plays the beep, counts the press in
test.presses and draws a random number into test.random (the engine's
Rand, the random seed's). Start saves the count in the game's save
(saveRAM[0], WriteSaveRAM); C reads the save back (ReadSaveRAM) into
test.saved. The project runs it with Skip Start Menu, as the game has no
menus of its own. It draws itself as a rectangle whose colour
changes with the presses, and a second rectangle as wide as the random
number, so each shows in the picture.
"""
import hashlib
import struct
import sys
import os

HERE = os.path.dirname(os.path.abspath(__file__))

def pack(files):
    """files: {"Data/Dir/Name": bytes} -> the Data.rsdk bytes"""
    names = sorted(files)
    header = 6 + 2 + 24 * len(names)
    out = bytearray(b"RSDKvB" + struct.pack("<H", len(names)))
    body = bytearray()
    for name in names:
        dg = hashlib.md5(name.lower().encode()).digest()
        out += b"".join(dg[4 * i:4 * i + 4][::-1] for i in range(4))
        out += struct.pack("<II", header + len(body), len(files[name]))
        body += files[name]
    return bytes(out + body)


def s8(text):
    b = text.encode()
    return bytes([len(b)]) + b


def game_config():
    out = s8("Chimera Test") + s8("A game made for the core's tests")
    for i in range(0x60):                               # the global palette
        out += bytes([(i * 37) & 0xFF, (i * 91) & 0xFF, (i * 53) & 0xFF])
    out += bytes([0])                                   # no global objects
    variables = [("test.presses", 0), ("test.random", 0), ("test.saved", -1)]
    out += bytes([len(variables)])
    for name, value in variables:
        out += s8(name) + struct.pack(">i", value)
    out += bytes([1]) + s8("Beep") + s8("Test/Beep.wav")   # global sound effects: names, then paths
    out += bytes([0])                                   # players
    # the four stage lists in the file's order: presentation, regular,
    # special, bonus; each stage: folder, act id, name, highlighted
    out += bytes([1]) + s8("Test") + s8("1") + s8("TEST STAGE") + bytes([0])
    out += bytes([0]) + bytes([0]) + bytes([0])
    return out


def stage_config():
    out = bytes([0])                                    # no global scripts
    for i in range(32):                                 # palette 0x60..0x7F
        out += bytes([i * 8, 255 - i * 8, 128])
    out += bytes([0])                                   # no stage sound effects
    out += bytes([1]) + s8("Box")                       # one object type
    out += s8("Test/Box.txt")                           # its script
    return out


def act():
    out = s8("TEST")
    out += bytes([9, 9, 9, 9]) + bytes([3])             # no tile layers drawn
    out += bytes([1, 0, 1, 0]) + b"\x00\x00"            # a 1x1 layout, empty
    out += struct.pack("<H", 1)                         # one object
    out += struct.pack("<HBBii", 0, 1, 0, 100 << 16, 120 << 16)  # Box at (100, 120)
    return out


BOX = """// Box: the core's test object
private alias object.value0 : object.frames
private alias object.value1 : object.colour

event ObjectUpdate
	object.frames++
	if keyDown.left == true
		object.xpos -= 0x20000
	end if
	if keyDown.right == true
		object.xpos += 0x20000
	end if
	if keyPress.buttonA == true
		PlaySfx(0, 0)
		test.presses++
		Rand(temp0, 200)
		test.random = temp0
		object.colour += 40
	end if
	if keyPress.start == true
		saveRAM[0] = test.presses
		WriteSaveRAM()
	end if
	if keyPress.buttonC == true
		saveRAM[0] = 0
		ReadSaveRAM()
		test.saved = saveRAM[0]
	end if
end event

event ObjectDraw
	ClearScreen(0)
	temp1 = object.colour
	temp1 &= 255
	DrawRect(object.ixpos, object.iypos, 16, 16, 255, temp1, 0, 255)
	DrawRect(8, 8, test.random, 4, 255, 255, 255, 255)
end event

event ObjectStartup
	stage.timeEnabled = true
	SetMusicTrack("Tune.ogg", 0, 1)
	PlayMusic(0)
end event
"""


def beep():
    rate, n = 44100, 4410
    samples = b"".join(struct.pack("<h", 6000 if (i // 50) % 2 else -6000) for i in range(n))
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate * 2, 2, 16)
    data = b"RIFF" + struct.pack("<I", 36 + len(samples)) + b"WAVE"
    data += b"fmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(samples)) + samples
    return data


def main():
    files = {
        "Data/Game/GameConfig.bin": game_config(),
        "Data/SoundFX/Test/Beep.wav": beep(),
        "Data/Music/Tune.ogg": open(os.path.join(HERE, "tune.ogg"), "rb").read(),
        "Data/Stages/Test/StageConfig.bin": stage_config(),
        "Data/Stages/Test/Act1.bin": act(),
        "Data/Scripts/Test/Box.txt": BOX.replace("\n", "\r\n").encode(),
    }
    open(sys.argv[1], "wb").write(pack(files))


if __name__ == "__main__":
    main()
