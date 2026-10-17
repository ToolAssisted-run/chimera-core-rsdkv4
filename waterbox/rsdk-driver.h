/* rsdk-driver.h - Retro Engine v4 (the RSDKv4 decompilation) as a machine
 * that is stepped.
 *
 * The same driver is compiled for the miniBox guest and for the native
 * reference (run-native); wbx-entry.c puts the guest ABI on top of it.
 *
 * Wire format (waterbox.config "input.buttons", same order): the engine's
 * fourteen inputs - the pad's four directions, A, B, C, X, Y, Z, L, R,
 * Start and Select. Sonic 1 and 2 read the directions, A, B and C (each
 * jumps) and Start; their menus also read the others. On a PC keyboard they
 * are the keys the decompilation's own settings.ini gives them (the arrows,
 * Z X C, A S D, Q E, Enter and Tab), which is what the core presses.
 */
#ifndef RSDK_DRIVER_H
#define RSDK_DRIVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum RsdkButton
{
	RSDK_BTN_UP,
	RSDK_BTN_DOWN,
	RSDK_BTN_LEFT,
	RSDK_BTN_RIGHT,
	RSDK_BTN_A,
	RSDK_BTN_B,
	RSDK_BTN_C,
	RSDK_BTN_X,
	RSDK_BTN_Y,
	RSDK_BTN_Z,
	RSDK_BTN_L,
	RSDK_BTN_R,
	RSDK_BTN_START,
	RSDK_BTN_SELECT,
	RSDK_BTN_COUNT
};

/* the picture: what the engine's OpenGL draws (its software screen as a
 * texture, its menus and touch controls on top), 240 lines and as wide as
 * the screenWidth setting (424 by default), times the renderScale setting */
#define RSDK_VIDEO_HEIGHT 240
#define RSDK_MAX_SCALE 2
#define RSDK_VIDEO_MAX_WIDTH (424 * RSDK_MAX_SCALE)
#define RSDK_VIDEO_MAX_HEIGHT (240 * RSDK_MAX_SCALE)

/* stereo, 44100 frames a second (the engine's mixing rate); one step is one
 * sixtieth of a second, 735 frames */
#define RSDK_AUDIO_RATE 44100
#define RSDK_FRAME_RATE 60
#define RSDK_AUDIO_SAMPLES (RSDK_AUDIO_RATE / RSDK_FRAME_RATE)

/* 0 on failure, with the reason in err */
/* the button after the engine's inputs: the PC keyboard's Escape, which
 * opens the engine's developer menu when the Developer Menu setting is on
 * (it is live only then) */
#define RSDK_BTN_DEV_MENU RSDK_BTN_COUNT

int rsdkdrv_init(char *err, int errsize);
void rsdkdrv_set_button(int index, int down);
int rsdkdrv_dev_menu(void);   /* the Developer Menu setting */
/* runs the engine through its next frame */
void rsdkdrv_frame(int render);
const uint32_t *rsdkdrv_video(int *w, int *h);
const int16_t *rsdkdrv_audio(int *samples);   /* stereo frames, left then right */
int rsdkdrv_input_was_read(void);
uint64_t rsdkdrv_frames(void);

/* game-state: the property block, the raw domains, the table */
int rsdkdrv_domain_count(void);
const char *rsdkdrv_domain_name(int i);
uint8_t *rsdkdrv_domain_ptr(int i);
int64_t rsdkdrv_domain_size(int i);
int rsdkdrv_domain_writable(int i);
const char *rsdkdrv_game_properties(void);

/* rsdk-files.c: the game's data, the settings file the engine reads, its
 * saves */
struct rsdk_settings
{
	int language;        /* RETRO_EN .. RETRO_JP */
	int game;            /* 1: Sonic 1, 2: Sonic 2 */
	int screen_width;    /* 320 .. 424 */
	int render_scale;    /* 1 .. RSDK_MAX_SCALE: the OpenGL picture's size */
	int skip_start_menu; /* SkipStartMenu */
	long random_seed;
	int dev_menu;        /* DevMenu */
};
void rsdk_settings_read(struct rsdk_settings *s);
int rsdk_files_load(const struct rsdk_settings *s, char *err, int errsize);

/* The game's saves (SData.bin, SGame.bin) for Emulator > Export Save Data:
 * a snapshot of those that hold anything, then each one's name and bytes. */
int rsdk_saves_count(void);
const char *rsdk_saves_name(int index);
int64_t rsdk_saves_size(int index);
const uint8_t *rsdk_saves_data(int index);

#ifdef __cplusplus
}
#endif

#endif
