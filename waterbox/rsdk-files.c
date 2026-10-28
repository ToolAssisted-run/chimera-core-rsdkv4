/* rsdk-files.c - every file the engine opens, answered by the core.
 *
 * The engine opens its files through SDL's file reader, and SDL and the
 * C library open them with fopen; the link wraps both SDL_RWFromFile and
 * fopen (sources.mk), so each open lands here:
 *
 *   - Data.rsdk, the game's data pack: the file the project brings as its
 *     firmware (Sonic1.rsdk or Sonic2.rsdk, waterbox.config, by the game
 *     setting), copied once at Init into sealed memory - read-only after
 *     Init, and so in no savestate - and opened from there with fmemopen. The
 *     stream itself (its position, its buffer) is in the guest's heap, which a
 *     savestate carries; a host file handle would not survive a load.
 *   - settings.ini: written by the core from the project's settings, so the
 *     engine starts as the project says and never as a settings file left on
 *     some disk says. The engine's own attempts to write it back fail.
 *   - SData.bin and SGame.bin, the game's saves: in guest memory, empty when
 *     the machine starts, and carried by savestates like the rest of it.
 *   - anything else: not there. A write (log.txt, usage.txt) fails quietly,
 *     as on a read-only disk.
 *
 * Names are matched without regard to case, as the decompilation's
 * case-insensitive build does on disk.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL2/SDL.h>

#include <emulibc.h>
#include <waterbox_settings.h>
#include <waterbox_slots.h>

#include "rsdk-driver.h"

/* A text a person reads is a key of languages/en.json, each of its
 * arguments after a TEXT_ARG (chimera docs/translations.md). TEXT_ARG is a
 * string of its own so the byte after it is never read as more of its hex. */
#define TEXT_ARG "\x1f"

FILE *__real_fopen(const char *path, const char *mode);
int __real_fclose(FILE *f);

/* the data pack, sealed after Init */
static uint8_t *g_pack;
static size_t g_pack_size;

/* settings.ini, as the core wrote it */
static char g_ini[4096];
static size_t g_ini_len;

/* the saves: a stream being written is an open_memstream, whose buffer the C
 * library hands back at fclose; the last one closed is the file */
struct save_file
{
	const char *name;
	char *data;         /* the file's bytes (malloc'd: guest heap) */
	size_t size;
	char *writing;      /* open_memstream's, while a write is open */
	size_t writing_size;
	FILE *stream;
};
static struct save_file g_saves[] = { { "SData.bin" }, { "SGame.bin" } };
#define NSAVES (sizeof g_saves / sizeof g_saves[0])

/* ------------------------------------------------------------ settings */

void rsdk_settings_read(struct rsdk_settings *s)
{
	char str[16] = "sonic1";
	wbx_setting_str("game", str, (int)sizeof str);
	s->game = !strcmp(str, "sonic2") ? 2 : 1;
	snprintf(str, sizeof str, "en");
	wbx_setting_str("language", str, (int)sizeof str);
	static const char *const langs[] = { "en", "fr", "it", "de", "es", "ja", "pt", "ru", "ko", "zh", "zhs" };
	s->language = 0;
	for (int i = 0; i < (int)(sizeof langs / sizeof langs[0]); i++)
		if (!strcmp(str, langs[i])) s->language = i;
	s->screen_width = (int)wbx_setting_long("screenWidth", 424);
	s->render_scale = (int)wbx_setting_long("renderScale", 1);
	s->skip_start_menu = wbx_setting_bool("skipStartMenu", 0);
	s->random_seed = wbx_setting_long("randomSeed", 0);
	s->dev_menu = wbx_setting_bool("devMenu", 0);
}

/* the engine's settings file (Userdata.cpp InitUserdata reads it): the
 * keys it knows, the values the project's settings give. What is not a
 * setting is upstream's default, except where the default would make the
 * picture depend on the host: no full screen, no VSync, no dimming of an
 * idle screen (DimLimit -1: the dimming is the decompilation's addition, and
 * it counts frames without a press, which a movie would then have to carry),
 * no focus pause. The keys are upstream's defaults, by SDL scancode. */
static void write_ini(const struct rsdk_settings *s)
{
	int n = snprintf(g_ini, sizeof g_ini,
		"[Dev]\n"
		"DevMenu=%s\n"
		"EngineDebugMode=false\n"
		"TxtScripts=false\n"
		"StartingCategory=255\n"
		"StartingScene=255\n"
		"StartingPlayer=255\n"
		"StartingSaveFile=255\n"
		"FastForwardSpeed=8\n"
		"UseHQModes=true\n"
		"DataFile=Data.rsdk\n"
		"[Game]\n"
		"Language=%d\n"
		"SkipStartMenu=%s\n"
		"GameType=0\n"
		"DisableFocusPause=3\n"
		"[Window]\n"
		"FullScreen=false\n"
		"Borderless=false\n"
		"VSync=false\n"
		"ScalingMode=0\n"
		"WindowScale=%d\n"
		"ScreenWidth=%d\n"
		"RefreshRate=60\n"
		"DimLimit=-1\n"
		"[Audio]\n"
		"BGMVolume=1.0\n"
		"SFXVolume=1.0\n"
		"[Keyboard 1]\n"
		"Up=82\nDown=81\nLeft=80\nRight=79\nA=29\nB=27\nC=6\nX=4\nY=22\nZ=7\nL=20\nR=8\nStart=40\nSelect=43\n"
		"[Controller 1]\n"
		"Up=11\nDown=12\nLeft=13\nRight=14\nA=0\nB=1\nC=2\nX=3\nY=4\nZ=5\nL=9\nR=10\nStart=6\nSelect=4\n"
		"LStickDeadzone=0.3\nRStickDeadzone=0.3\nLTriggerDeadzone=0.3\nRTriggerDeadzone=0.3\n",
		s->dev_menu ? "true" : "false", s->language, s->skip_start_menu ? "true" : "false", s->render_scale, s->screen_width);
	g_ini_len = (size_t)n;
}

/* ------------------------------------------------------------ the pack */

/* the game's data file: the project's firmware for the game it plays
 * (waterbox.config), mounted under its id */
static int read_pack(const struct rsdk_settings *st, char *err, int errsize)
{
	const char *name = st->game == 2 ? "Sonic2.rsdk" : "Sonic1.rsdk";
	FILE *f = __real_fopen(name, "rb");
	if (!f)
	{
		snprintf(err, (size_t)errsize, "error.game_s_data_file_is_not" TEXT_ARG "%s", name);
		return 0;
	}
	fseek(f, 0, SEEK_END);
	const long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 6)
	{
		fclose(f);
		snprintf(err, (size_t)errsize, "error.is_not_a_retro_engine_data" TEXT_ARG "%s" TEXT_ARG "%ld", name, size);
		return 0;
	}
	g_pack = (uint8_t *)alloc_sealed((size_t)size);
	if (!g_pack)
	{
		fclose(f);
		snprintf(err, (size_t)errsize, "error.is_too_large_bytes_for_the" TEXT_ARG "%s" TEXT_ARG "%ld", name, size);
		return 0;
	}
	const size_t got = fread(g_pack, 1, (size_t)size, f);
	fclose(f);
	if (got != (size_t)size)
	{
		snprintf(err, (size_t)errsize, "error.could_not_be_read_whole" TEXT_ARG "%s", name);
		return 0;
	}
	g_pack_size = (size_t)size;
	return 1;
}

/* The project's save data (the savedata slot, chimera#302): SData.bin and
 * SGame.bin as Export Save Data wrote them, put in place here at Init, before
 * the seal, so they are part of the machine as it starts. A name the game
 * does not keep fails the load: progress silently ignored is worse than a
 * project that will not start. */
static int seed_saves(char *err, int errsize)
{
	char name[256];
	for (int i = 0; wbx_slot_name("savedata", i, name, (int)sizeof name); i++)
	{
		struct save_file *sv = NULL;
		for (size_t k = 0; k < NSAVES; k++)
			if (!strcasecmp(name, g_saves[k].name)) sv = &g_saves[k];
		if (!sv)
		{
			snprintf(err, (size_t)errsize, "error.save_data_is_not_a_file_this" TEXT_ARG "%s", name);
			return 0;
		}
		FILE *f = __real_fopen(name, "rb");
		if (!f)
		{
			snprintf(err, (size_t)errsize, "error.save_data_could_not_be" TEXT_ARG "%s", name);
			return 0;
		}
		size_t cap = 0, n = 0;
		char *data = NULL;
		for (;;)
		{
			if (n == cap)
			{
				cap = cap ? cap * 2 : 65536;
				data = (char *)realloc(data, cap);
			}
			const size_t got = fread(data + n, 1, cap - n, f);
			n += got;
			if (got == 0) break;
		}
		__real_fclose(f);
		free(sv->data);
		sv->data = data;
		sv->size = n;
	}
	return 1;
}

int rsdk_files_load(const struct rsdk_settings *s, char *err, int errsize)
{
	write_ini(s);
	return read_pack(s, err, errsize) && seed_saves(err, errsize);
}

/* ------------------------------------------------------------ export */

static int g_export[NSAVES];
static int g_export_count;

int rsdk_saves_count(void)
{
	g_export_count = 0;
	for (size_t i = 0; i < NSAVES; i++)
		if (g_saves[i].data && g_saves[i].size) g_export[g_export_count++] = (int)i;
	return g_export_count;
}
const char *rsdk_saves_name(int index) { return index >= 0 && index < g_export_count ? g_saves[g_export[index]].name : ""; }
int64_t rsdk_saves_size(int index) { return index >= 0 && index < g_export_count ? (int64_t)g_saves[g_export[index]].size : 0; }
const uint8_t *rsdk_saves_data(int index) { return index >= 0 && index < g_export_count ? (const uint8_t *)g_saves[g_export[index]].data : NULL; }


/* ------------------------------------------------------------ fopen */

static const char *base_name(const char *path)
{
	const char *b = strrchr(path, '/');
	return b ? b + 1 : path;
}

static int writes(const char *mode) { return strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'); }

static FILE *open_save(struct save_file *sv, const char *mode)
{
	if (writes(mode))
	{
		if (sv->stream) return NULL; /* one writer at a time */
		free(sv->writing);
		sv->writing = NULL;
		sv->writing_size = 0;
		sv->stream = open_memstream(&sv->writing, &sv->writing_size);
		return sv->stream;
	}
	if (!sv->data || !sv->size) { errno = ENOENT; return NULL; }
	return fmemopen(sv->data, sv->size, "rb");
}

FILE *__wrap_fopen(const char *path, const char *mode)
{
	if (!path || !mode) { errno = EINVAL; return NULL; }
	const char *name = base_name(path);
	for (size_t i = 0; i < NSAVES; i++)
		if (!strcasecmp(name, g_saves[i].name)) return open_save(&g_saves[i], mode);
	if (writes(mode)) { errno = EROFS; return NULL; }
	/* the core's own channels, read by the miniBox kit at Init */
	if (!strcmp(path, "settings") || !strcmp(path, "slots")) return __real_fopen(path, mode);
	if (!strcasecmp(path, "settings.ini") && g_ini_len) return fmemopen(g_ini, g_ini_len, "rb");
	if (!strcasecmp(path, "Data.rsdk") && g_pack) return fmemopen(g_pack, g_pack_size, "rb");
	errno = ENOENT;
	return NULL;
}

/* The engine opens its files through SDL (Reader.hpp: FileIO is an
 * SDL_RWops), and SDL's SDL_RWFromFile refuses a stream that is not a
 * regular file or a pipe - a memory stream is neither, having no file
 * descriptor. So SDL is answered here too, with the same stream wrapped as
 * SDL would wrap a file's: closing it closes the stream (and a save's
 * close, below, keeps its bytes). */
SDL_RWops *__wrap_SDL_RWFromFile(const char *file, const char *mode)
{
	FILE *f = __wrap_fopen(file, mode);
	if (!f)
	{
		SDL_SetError("Couldn't open %s", file ? file : "(null)");
		return NULL;
	}
	return SDL_RWFromFP(f, SDL_TRUE);
}

/* a save written and closed: its bytes become the file */
int __wrap_fclose(FILE *f)
{
	for (size_t i = 0; i < NSAVES; i++)
	{
		struct save_file *sv = &g_saves[i];
		if (f && f == sv->stream)
		{
			const int r = __real_fclose(f);
			sv->stream = NULL;
			free(sv->data);
			sv->data = sv->writing;
			sv->size = sv->writing_size;
			sv->writing = NULL;
			sv->writing_size = 0;
			return r;
		}
	}
	return __real_fclose(f);
}
