/* game-state.cpp - what the tools see of the game: chimera docs/game-cores.md.
 *
 * Three memory domains:
 *
 *   - "Game State": the core's own block, copied from the engine after every
 *     step. The engine's mode, the stage and its timer: read-only, as the
 *     engine derives them.
 *   - "Objects": the engine's object list (objectEntityList), in place and
 *     writable: every object of the stage. In Sonic 1 and 2 the players are
 *     objects 0 and 1, so "Player 1" and "Player 2" name those two.
 *   - "Global Variables": the game's global variables, in place and writable,
 *     each a property named as the game names it (its GameConfig.bin), so a
 *     watch reads "player.score" or "player.lives" whatever the game is.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "RetroEngine.hpp"

#include "rsdk-driver.h"

namespace {

struct __attribute__((packed)) GameState
{
	uint64_t frames;          /* 0 */
	uint8_t game_mode;        /* 8: Engine.gameMode */
	uint8_t stage_mode;       /* 9 */
	uint8_t stage_list;       /* 10: activeStageList */
	uint8_t stage_position;   /* 11: stageListPosition */
	int32_t act;              /* 12 */
	uint32_t stage_time_ms;   /* 16: the stage timer, in milliseconds */
	int32_t frame_counter;    /* 20 */
	uint8_t time_enabled;     /* 24 */
	uint8_t pad[7];
	char stage_folder[64];    /* 32 */
	char stage_name[64];      /* 96 */
};

GameState gs;

} // namespace

extern "C" {

void gamestate_from_game(void)
{
	gs.frames = rsdkdrv_frames();
	gs.game_mode = (uint8_t)Engine.gameMode;
	gs.stage_mode = (uint8_t)stageMode;
	gs.stage_list = (uint8_t)activeStageList;
	gs.stage_position = (uint8_t)stageListPosition;
	gs.act = actID;
	gs.stage_time_ms = (uint32_t)(stageMinutes * 60000 + stageSeconds * 1000 + stageMilliseconds * 10);
	gs.frame_counter = frameCounter;
	gs.time_enabled = timeEnabled ? 1 : 0;
	memset(gs.stage_folder, 0, sizeof gs.stage_folder);
	memset(gs.stage_name, 0, sizeof gs.stage_name);
	if (activeStageList >= 0 && activeStageList < STAGELIST_MAX && stageListPosition >= 0 && stageListPosition < 0x100)
	{
		const SceneInfo &si = stageList[activeStageList][stageListPosition];
		snprintf(gs.stage_folder, sizeof gs.stage_folder, "%s", si.folder);
		snprintf(gs.stage_name, sizeof gs.stage_name, "%s", si.name);
	}
}

int rsdkdrv_domain_count(void) { return 3; }

const char *rsdkdrv_domain_name(int i)
{
	static const char *const names[] = { "Game State", "Objects", "Global Variables" };
	return i >= 0 && i < 3 ? names[i] : "";
}

uint8_t *rsdkdrv_domain_ptr(int i)
{
	switch (i)
	{
	case 0: return (uint8_t *)&gs;
	case 1: return (uint8_t *)objectEntityList;
	case 2: return (uint8_t *)globalVariables;
	}
	return NULL;
}

int64_t rsdkdrv_domain_size(int i)
{
	switch (i)
	{
	case 0: return (int64_t)sizeof gs;
	case 1: return (int64_t)sizeof objectEntityList;
	case 2: return (int64_t)sizeof globalVariables;
	}
	return 0;
}

int rsdkdrv_domain_writable(int i) { return i >= 1 && i <= 2; }

namespace {

const struct { const char *name; size_t off; const char *type; int count; const char *desc; } k_entity[] = {
	{ "X", offsetof(Entity, xpos), "s32", 1, "property.entity.x" },
	{ "Y", offsetof(Entity, ypos), "s32", 1, "property.entity.x" },
	{ "X Velocity", offsetof(Entity, xvel), "s32", 1, "" },
	{ "Y Velocity", offsetof(Entity, yvel), "s32", 1, "" },
	{ "Speed", offsetof(Entity, speed), "s32", 1, "property.entity.speed" },
	{ "Values", offsetof(Entity, values), "s32", 48, "" },
	{ "State", offsetof(Entity, state), "s32", 1, "" },
	{ "Angle", offsetof(Entity, angle), "s32", 1, "" },
	{ "Scale", offsetof(Entity, scale), "s32", 1, "" },
	{ "Rotation", offsetof(Entity, rotation), "s32", 1, "" },
	{ "Alpha", offsetof(Entity, alpha), "s32", 1, "" },
	{ "Animation Timer", offsetof(Entity, animationTimer), "s32", 1, "" },
	{ "Animation Speed", offsetof(Entity, animationSpeed), "s32", 1, "" },
	{ "Look Pos X", offsetof(Entity, lookPosX), "s32", 1, "" },
	{ "Look Pos Y", offsetof(Entity, lookPosY), "s32", 1, "" },
	{ "Group", offsetof(Entity, groupID), "u16", 1, "" },
	{ "Type", offsetof(Entity, type), "u8", 1, "" },
	{ "Property Value", offsetof(Entity, propertyValue), "u8", 1, "" },
	{ "Priority", offsetof(Entity, priority), "u8", 1, "" },
	{ "Draw Order", offsetof(Entity, drawOrder), "u8", 1, "" },
	{ "Direction", offsetof(Entity, direction), "u8", 1, "" },
	{ "Ink Effect", offsetof(Entity, inkEffect), "u8", 1, "" },
	{ "Animation", offsetof(Entity, animation), "u8", 1, "" },
	{ "Previous Animation", offsetof(Entity, prevAnimation), "u8", 1, "" },
	{ "Frame", offsetof(Entity, frame), "u8", 1, "" },
	{ "Collision Mode", offsetof(Entity, collisionMode), "u8", 1, "" },
	{ "Collision Plane", offsetof(Entity, collisionPlane), "u8", 1, "" },
	{ "Control Mode", offsetof(Entity, controlMode), "s8", 1, "" },
	{ "Control Lock", offsetof(Entity, controlLock), "u8", 1, "" },
	{ "Pushing", offsetof(Entity, pushing), "u8", 1, "" },
	{ "Visible", offsetof(Entity, visible), "u8", 1, "" },
	{ "Tile Collisions", offsetof(Entity, tileCollisions), "u8", 1, "" },
	{ "Object Interactions", offsetof(Entity, objectInteractions), "u8", 1, "" },
	{ "Gravity", offsetof(Entity, gravity), "u8", 1, "property.entity.gravity" },
	{ "Left", offsetof(Entity, left), "u8", 1, "property.entity.left" },
	{ "Right", offsetof(Entity, right), "u8", 1, "" },
	{ "Up", offsetof(Entity, up), "u8", 1, "" },
	{ "Down", offsetof(Entity, down), "u8", 1, "" },
	{ "Jump Press", offsetof(Entity, jumpPress), "u8", 1, "" },
	{ "Jump Hold", offsetof(Entity, jumpHold), "u8", 1, "" },
	{ "Scroll Tracking", offsetof(Entity, scrollTracking), "u8", 1, "" },
};

} // namespace

/* the table: the block, the two players (objects 0 and 1), every object as an
 * array of structures, and the game's global variables by name. Built once,
 * after Init, when the game's configuration has named its variables. */
const char *rsdkdrv_game_properties(void)
{
	static char json[131072];
	if (json[0]) return json;
	int n = 0;
#define P(...) n += snprintf(json + n, sizeof json - (size_t)n, __VA_ARGS__)
#define GS(name, off, type, extra) \
	P("    { \"name\": \"%s\", \"domain\": \"Game State\", \"offset\": %d, \"type\": \"%s\", \"writable\": false%s },\n", name, off, type, extra)
	P("{\n  \"gameTimer\": \"Stage.Time Ms\",\n  \"properties\": [\n");
	GS("Engine.Frames", 0, "u64", ", \"group\": \"Engine\"");
	GS("Engine.Mode", 8, "u8", ", \"group\": \"Engine\"");
	GS("Stage.Mode", 9, "u8", ", \"group\": \"Stage\", \"values\": { \"0\": \"Load\", \"1\": \"Normal\", \"2\": \"Paused\", \"3\": \"Frozen\" }");
	GS("Stage.List", 10, "u8", ", \"group\": \"Stage\", \"values\": { \"0\": \"Presentation\", \"1\": \"Regular\", \"2\": \"Bonus\", \"3\": \"Special\" }");
	GS("Stage.Position", 11, "u8", ", \"group\": \"Stage\"");
	GS("Stage.Act", 12, "s32", ", \"group\": \"Stage\"");
	GS("Stage.Time Ms", 16, "u32", ", \"group\": \"Stage\", \"description\": \"property.stage_time_ms\"");
	GS("Stage.Frame Counter", 20, "s32", ", \"group\": \"Stage\"");
	GS("Stage.Time Enabled", 24, "bool", ", \"group\": \"Stage\"");
	P("    { \"name\": \"Stage.Folder\", \"domain\": \"Game State\", \"offset\": 32, \"type\": \"string\", \"length\": 64, \"writable\": false, \"group\": \"Stage\" },\n");
	P("    { \"name\": \"Stage.Name\", \"domain\": \"Game State\", \"offset\": 96, \"type\": \"string\", \"length\": 64, \"writable\": false, \"group\": \"Stage\" },\n");

	for (int p = 0; p < 2; p++)
		for (size_t i = 0; i < sizeof k_entity / sizeof k_entity[0]; i++)
		{
			const int off = p * (int)sizeof(Entity) + (int)k_entity[i].off;
			P("    { \"name\": \"Player %d.%s\", \"domain\": \"Objects\", \"offset\": %d, \"type\": \"%s\"", p + 1, k_entity[i].name, off, k_entity[i].type);
			if (k_entity[i].count > 1) P(", \"count\": %d", k_entity[i].count);
			P(", \"group\": \"Player %d\"", p + 1);
			if (k_entity[i].desc[0]) P(", \"description\": \"%s\"", k_entity[i].desc);
			P(" },\n");
		}
	const int objects = (int)(sizeof objectEntityList / sizeof objectEntityList[0]);
	for (size_t i = 0; i < sizeof k_entity / sizeof k_entity[0]; i++)
	{
		if (k_entity[i].count == 1)
			P("    { \"name\": \"Object.%s\", \"domain\": \"Objects\", \"offset\": %d, \"type\": \"%s\", \"count\": %d, \"stride\": %d, \"group\": \"Objects\" },\n",
			  k_entity[i].name, (int)k_entity[i].off, k_entity[i].type, objects, (int)sizeof(Entity));
		else
			for (int v = 0; v < k_entity[i].count; v++)
				P("    { \"name\": \"Object.%s %d\", \"domain\": \"Objects\", \"offset\": %d, \"type\": \"%s\", \"count\": %d, \"stride\": %d, \"group\": \"Objects\" },\n",
				  k_entity[i].name, v, (int)k_entity[i].off + 4 * v, k_entity[i].type, objects, (int)sizeof(Entity));
	}

	/* the game's own names; one the JSON could not carry as it is, or a
	 * second of the same name, is left out */
	for (int v = 0; v < globalVariablesCount && v < GLOBALVAR_COUNT; v++)
	{
		const char *name = globalVariableNames[v];
		int ok = name[0] != 0;
		for (const char *c = name; *c && ok; c++) ok = *c >= 0x20 && *c < 0x7F && *c != '"' && *c != '\\';
		for (int w = 0; w < v && ok; w++) ok = strcmp(globalVariableNames[w], name) != 0;
		if (!ok) continue;
		P("    { \"name\": \"%s\", \"domain\": \"Global Variables\", \"offset\": %d, \"type\": \"s32\", \"group\": \"Global Variables\" },\n", name, v * 4);
	}
	P("    { \"name\": \"Global\", \"domain\": \"Global Variables\", \"offset\": 0, \"type\": \"s32\", \"count\": %d, \"group\": \"Global Variables\", "
	  "\"description\": \"The game's global variables, by number\" }\n", GLOBALVAR_COUNT);
	P("  ]\n}\n");
#undef GS
#undef P
	return json;
}

} // extern "C"
