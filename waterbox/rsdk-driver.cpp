/* rsdk-driver.cpp - the Retro Engine v4 decompilation as a machine that is
 * stepped.
 *
 * The engine's own main loop (RetroEngine::Run) reads events, runs the game
 * for a frame (its native objects, one of which runs the stage), draws it,
 * and waits for the next sixtieth of a second. The core runs the same pass
 * without the wait (RunFrame, patches/0001), once a step:
 *
 *   - A step is one frame of the engine: one sixtieth of a second of the
 *     machine's time. The machine's clock is the number of frames run, in
 *     microseconds (seams.c answers SDL's counters from it).
 *   - The sound is the engine's own mixer, run once a frame for exactly 735
 *     stereo samples, the frame's share of 44100 a second - by the driver,
 *     in place of the sound device's thread.
 *   - The controls are the engine's keyboard: the buttons hold the keys
 *     settings.ini maps to the engine's fourteen inputs (rsdk-files.c), and
 *     the engine reads SDL's keyboard state at the start of the frame.
 *   - The picture is what the engine's OpenGL drew - its software screen as
 *     a texture, the menus and touch controls over it - read back from
 *     OSMesa's buffer when the engine swaps (seams.c).
 *
 * The engine is upstream's, built from source with one patch, its software
 * renderer for the stages and its OpenGL renderer (on Mesa's softpipe) for
 * the rest, and no mod loader and no networking (docs/PLAN.md).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "RetroEngine.hpp"

#include <GL/osmesa.h>

#include "rsdk-driver.h"

namespace {

struct Driver
{
	struct rsdk_settings settings;
	int init_done;

	uint8_t held[RSDK_BTN_COUNT];
	uint8_t keys[SDL_NUM_SCANCODES];

	uint64_t frames;
	int render;
	int read;

	SDL_AudioCallback audio_cb;
	void *audio_user;
	int16_t audio[RSDK_AUDIO_SAMPLES * 2];
	int audio_frames;

	/* OSMesa's colour buffer (RGBA, top row first) and the picture */
	OSMesaContext gl;
	uint8_t gl_buffer[RSDK_VIDEO_MAX_WIDTH * RSDK_VIDEO_MAX_HEIGHT * 4];
	int gl_w, gl_h;
	uint32_t video[RSDK_VIDEO_MAX_WIDTH * RSDK_VIDEO_MAX_HEIGHT];
	int video_w, video_h;
};

Driver g;

/* the scancode settings.ini maps to each button (rsdk-files.c write_ini):
 * upstream's own defaults */
const int k_scancodes[RSDK_BTN_COUNT] = {
	SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
	SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D,
	SDL_SCANCODE_Q, SDL_SCANCODE_E, SDL_SCANCODE_RETURN, SDL_SCANCODE_TAB,
};

} // namespace

/* ------------------------------------------------------------ seams.c */

extern "C" uint64_t rsdkdrv_clock_us(void) { return g.frames * 1000000ull / RSDK_FRAME_RATE; }
extern "C" long rsdkdrv_random_seed(void) { return g.settings.random_seed; }
extern "C" const uint8_t *rsdkdrv_keyboard_state(void) { return g.keys; }
extern "C" void rsdkdrv_audio_open(SDL_AudioCallback cb, void *user)
{
	g.audio_cb = cb;
	g.audio_user = user;
}

/* the window's OpenGL context: OSMesa's, on the driver's buffer, rows from
 * the top */
extern "C" int rsdkdrv_gl_create(int w, int h)
{
	if (w <= 0 || h <= 0 || w > RSDK_VIDEO_MAX_WIDTH || h > RSDK_VIDEO_MAX_HEIGHT) return 0;
	g.gl = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
	if (!g.gl) return 0;
	if (!OSMesaMakeCurrent(g.gl, g.gl_buffer, GL_UNSIGNED_BYTE, w, h)) return 0;
	OSMesaPixelStore(OSMESA_Y_UP, 0);
	g.gl_w = w;
	g.gl_h = h;
	return 1;
}

/* the frame drawn: what OpenGL drew is the picture (the size kept whether
 * converted or not) */
extern "C" void rsdkdrv_gl_swap(void)
{
	glFinish();
	g.video_w = g.gl_w;
	g.video_h = g.gl_h;
	if (!g.render) return;
	const uint8_t *src = g.gl_buffer;
	for (int i = 0; i < g.gl_w * g.gl_h; i++, src += 4)
		g.video[i] = 0xFF000000u | ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
}

/* ------------------------------------------------------------ the exports */

extern "C" {

void gamestate_from_game(void);

int rsdkdrv_init(char *err, int errsize)
{
	for (int i = 0; i < RSDK_VIDEO_MAX_WIDTH * RSDK_VIDEO_MAX_HEIGHT; i++) g.video[i] = 0xFF000000u;
	rsdk_settings_read(&g.settings);
	if (g.settings.screen_width < 320 || g.settings.screen_width > 424)
	{
		snprintf(err, (size_t)errsize, "the setting screenWidth is %d; it goes from 320 to 424", g.settings.screen_width);
		return 0;
	}
	if (g.settings.render_scale < 1 || g.settings.render_scale > RSDK_MAX_SCALE)
	{
		snprintf(err, (size_t)errsize, "the setting renderScale is %d; it goes from 1 to %d", g.settings.render_scale, RSDK_MAX_SCALE);
		return 0;
	}
	g.video_w = g.settings.screen_width * g.settings.render_scale;
	g.video_h = RSDK_VIDEO_HEIGHT * g.settings.render_scale;
	if (!rsdk_files_load(&g.settings, err, errsize)) return 0;

	g.render = 1;
	Engine.Init();
	if (!Engine.running || !Engine.initialised)
	{
		snprintf(err, (size_t)errsize, "the engine did not start: %s",
		         !Engine.usingDataFile_Config ? "the data file is not a Retro Engine v4 data file"
		         : CheckFileInfo("data/game/gameconfig.bin") < 0 ? "the data file has no Data/Game/GameConfig.bin"
		         : !g.gl ? "its OpenGL could not be made"
		                 : "the game's configuration could not be read");
		return 0;
	}
	if (!g.audio_cb)
	{
		snprintf(err, (size_t)errsize, "the engine did not open its sound device");
		return 0;
	}
	g.init_done = 1;
	gamestate_from_game();
	return 1;
}

void rsdkdrv_set_button(int index, int down)
{
	if (index >= 0 && index < RSDK_BTN_COUNT) g.held[index] = down ? 1 : 0;
}

void rsdkdrv_frame(int render)
{
	g.render = render;
	g.audio_frames = 0;
	if (!Engine.running) return;
	for (int i = 0; i < RSDK_BTN_COUNT; i++) g.keys[k_scancodes[i]] = g.held[i];
	/* a frame reads the controls unless it is the one in which the engine
	 * loads a stage (ProcessStage's STAGEMODE_LOAD: files, scripts and the
	 * objects' startup, and no object runs) */
	g.read = !(Engine.gameMode == ENGINE_MAINGAME && stageMode == STAGEMODE_LOAD);
	Engine.RunFrame();
	g.frames++;
	memset(g.audio, 0, sizeof g.audio);
	g.audio_cb(g.audio_user, (Uint8 *)g.audio, (int)sizeof g.audio);
	g.audio_frames = RSDK_AUDIO_SAMPLES;
	gamestate_from_game();
}

const uint32_t *rsdkdrv_video(int *w, int *h)
{
	*w = g.video_w;
	*h = g.video_h;
	return g.video;
}

const int16_t *rsdkdrv_audio(int *samples)
{
	*samples = g.audio_frames;
	return g.audio;
}

/* whether the frame just run read the controls: the engine reads them at
 * the start of every frame (ProcessInput) and hands them to the game, except
 * in the frame it loads a stage; a stopped engine reads nothing */
int rsdkdrv_input_was_read(void) { return Engine.running ? g.read : 0; }

uint64_t rsdkdrv_frames(void) { return g.frames; }

} // extern "C"
