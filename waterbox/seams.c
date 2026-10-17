/* seams.c - what the engine reaches through SDL and the C library, answered
 * by the core.
 *
 * Linked with -Wl,--wrap=<name> for each __wrap_ below, in both builds
 * (sources.mk). SDL is the real SDL 2, built from source in its minimal
 * configuration (the dummy video driver, no threads, no timers, no CPU
 * feature dispatch); the engine uses it as it is, but for:
 *
 *   - time: the engine paces itself with SDL's performance counter, which is
 *     the machine's clock, which only a step moves (rsdk-driver.cpp). time()
 *     is asked once, for the random seed, and answers the project's
 *     randomSeed setting.
 *   - rand and srand: musl's generator, in both builds (glibc's rand is
 *     another sequence), its state in guest memory.
 *   - SDL_Init: everything a build without threads cannot give is left out;
 *     what is left is the video driver (dummy) and events.
 *   - the sound device: never opened. The engine's mixer is called by the
 *     driver for exactly one frame's samples at the end of each frame.
 *   - the keyboard: the state of the fourteen keys the buttons hold.
 *   - OpenGL: the window has none; the context is OSMesa's (below).
 */
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "rsdk-driver.h"

/* rsdk-driver.cpp */
uint64_t rsdkdrv_clock_us(void);
long rsdkdrv_random_seed(void);
const uint8_t *rsdkdrv_keyboard_state(void);
void rsdkdrv_audio_open(SDL_AudioCallback cb, void *user);
int rsdkdrv_gl_create(int w, int h);
void rsdkdrv_gl_swap(void);

/* ----------------------------------------------------------------- time */

Uint64 __wrap_SDL_GetPerformanceCounter(void) { return rsdkdrv_clock_us(); }
Uint64 __wrap_SDL_GetPerformanceFrequency(void) { return 1000000; }
Uint32 __wrap_SDL_GetTicks(void) { return (Uint32)(rsdkdrv_clock_us() / 1000); }
Uint64 __wrap_SDL_GetTicks64(void) { return rsdkdrv_clock_us() / 1000; }
void __wrap_SDL_Delay(Uint32 ms) { (void)ms; }

time_t __wrap_time(time_t *t)
{
	const time_t v = (time_t)rsdkdrv_random_seed();
	if (t) *t = v;
	return v;
}

static uint64_t g_rand_seed;
void __wrap_srand(unsigned s) { g_rand_seed = s - 1; }
int __wrap_rand(void)
{
	g_rand_seed = 6364136223846793005ULL * g_rand_seed + 1;
	return (int)(g_rand_seed >> 33);
}

/* ------------------------------------------------------------------- init */

int __real_SDL_Init(Uint32 flags);
int __real_SDL_InitSubSystem(Uint32 flags);
static const Uint32 k_unavailable = SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK |
	SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR;

int __wrap_SDL_Init(Uint32 flags)
{
	/* the dummy video driver, over whatever the environment says, so a host's
	 * SDL_VIDEODRIVER never reaches the native reference either */
	SDL_SetHintWithPriority(SDL_HINT_VIDEODRIVER, "dummy", SDL_HINT_OVERRIDE);
	return __real_SDL_Init(flags & ~k_unavailable);
}

int __wrap_SDL_InitSubSystem(Uint32 flags)
{
	if (flags & k_unavailable) return SDL_SetError("not in this machine");
	return __real_SDL_InitSubSystem(flags);
}

/* ------------------------------------------------------------------ audio */

/* the engine asks for 44100 Hz, signed 16-bit, stereo (Audio.cpp
 * InitAudioPlayback), which is what the core hands out */
SDL_AudioDeviceID __wrap_SDL_OpenAudioDevice(const char *device, int iscapture, const SDL_AudioSpec *desired,
                                             SDL_AudioSpec *obtained, int allowed_changes)
{
	(void)device; (void)iscapture; (void)allowed_changes;
	if (!desired || desired->freq != RSDK_AUDIO_RATE || desired->format != AUDIO_S16SYS || desired->channels != 2 || !desired->callback)
	{
		SDL_SetError("the core's sound device is 44100 Hz, 16-bit, stereo");
		return 0;
	}
	if (obtained)
	{
		*obtained = *desired;
		obtained->silence = 0;
	}
	rsdkdrv_audio_open(desired->callback, desired->userdata);
	return 1;
}

int __wrap_SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
	return __wrap_SDL_OpenAudioDevice(NULL, 0, desired, obtained, 0) ? 0 : -1;
}

void __wrap_SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) { (void)dev; (void)pause_on; }
void __wrap_SDL_PauseAudio(int pause_on) { (void)pause_on; }
void __wrap_SDL_CloseAudioDevice(SDL_AudioDeviceID dev) { (void)dev; }
void __wrap_SDL_CloseAudio(void) {}
void __wrap_SDL_LockAudio(void) {}
void __wrap_SDL_UnlockAudio(void) {}
void __wrap_SDL_LockAudioDevice(SDL_AudioDeviceID dev) { (void)dev; }
void __wrap_SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) { (void)dev; }

/* -------------------------------------------------------------- the keys */

const Uint8 *__wrap_SDL_GetKeyboardState(int *numkeys)
{
	if (numkeys) *numkeys = SDL_NUM_SCANCODES;
	return rsdkdrv_keyboard_state();
}

/* ------------------------------------------------------------ the screen
 *
 * The engine draws through OpenGL: its software screen as a texture, and its
 * menus and touch controls (native objects) on top. The OpenGL is Mesa's
 * softpipe behind OSMesa, compiled into the core (setup-mesa.sh), drawing
 * into a buffer in guest memory; SDL's window is a dummy without one. So
 * SDL's OpenGL calls are answered here: the window is made without OpenGL,
 * the context is an OSMesa one on the driver's buffer, and a swap finishes
 * the frame. Plain C with no JIT: every machine draws the same picture. */

SDL_Window *__real_SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags);
SDL_Window *__wrap_SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
	return __real_SDL_CreateWindow(title, x, y, w, h, flags & ~(Uint32)SDL_WINDOW_OPENGL);
}

SDL_GLContext __wrap_SDL_GL_CreateContext(SDL_Window *window)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(window, &w, &h);
	return rsdkdrv_gl_create(w, h) ? (SDL_GLContext)1 : NULL;
}
int __wrap_SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context) { (void)window; (void)context; return 0; }
void __wrap_SDL_GL_SwapWindow(SDL_Window *window) { (void)window; rsdkdrv_gl_swap(); }
int __wrap_SDL_GL_SetSwapInterval(int interval) { (void)interval; return 0; }
int __wrap_SDL_GL_SetAttribute(SDL_GLattr attr, int value) { (void)attr; (void)value; return 0; }
void __wrap_SDL_GL_DeleteContext(SDL_GLContext context) { (void)context; }
void __wrap_SDL_GL_GetDrawableSize(SDL_Window *window, int *w, int *h) { SDL_GetWindowSize(window, w, h); }
