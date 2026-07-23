#include <jni.h>
#include <dlfcn.h>
#include "Substrate.h"
#include <android/log.h>
#include <memory>
#include "main.h"
#include <sys/mman.h>
#include <vector>
#include <string>
#include <cmath>
#include "minIni/minIni.h"

struct soinfo2 {
	char name[128];
	const void* phdr;
	int phnum;
	unsigned entry;
	unsigned base;
	unsigned size;
};
static soinfo2* handle;

struct FullTile {
	uint8_t id;
	uint8_t data;
};

struct Boat {
	char pad[0x54];
	float yRot;
	char pad2[0xEC];
	float paddleAngle;
	float paddleStrength[2];
};

struct FoodItem {
	char pad[0x12];
	short id;
};

struct OreFeature {
	char pad[12];
	char id;
	char dmg;
};

struct Tile {
	char pad[0x12];
	short id;
};

struct ItemInstance {
	short count;
	short dmg;
	Tile* tile;
	void* item;
	bool valid;
};

struct Color {
	float red;
	float green;
	float blue;
	float alpha;
};

struct TilePos {
	int x;
	int y;
	int z;
};

bool v0_11_1_2 = false;
bool v0_11_1_1 = false;

static void (*origPlaceBlock)(FullTile*, void*, FullTile*);
static void hookPlaceBlock(FullTile* out, void* self, FullTile* in) {
	origPlaceBlock(out, self, in);
	if (out->id == 198) out->id = 13;
}

static void (*origSkinRepo)(void*, void*, void*, void*);
static void hookSkinRepo(void* self, void* gameStore, void* textures, void* path) {
	origSkinRepo(self, gameStore, textures, path);
	uint32_t* vecBegin  = *(uint32_t**)((uint8_t*)self + 0x10);
	uint32_t** vecEnd   =  (uint32_t**)((uint8_t*)self + 0x14);
	uint32_t** vecCap   =  (uint32_t**)((uint8_t*)self + 0x18);
	*vecEnd = (uint32_t*)((uint8_t*)vecBegin + 4);
}

static bool (*origShovelUseOn)(void*, void*, void*, int, int, int, signed char, float, float, float);
static bool hookShovelUseOn(void* self, void* item, void* player, int x, int y, int z, signed char face, float fx, float fy, float fz) {
	return false;
}

static float (*origPaddleForce)(Boat*, int);
static float hookPaddleForce(Boat* self, int side) {
	return self->paddleStrength[side] * 0.01375f * 2.25f;
}

static void (*origAddPaddleTime)(void*, int, float);
static void hookAddPaddleTime(void* self, int side, float amount) {
	float visualAmount = amount * 0.55f;
	origAddPaddleTime(self, side, visualAmount);
}

static int16_t (*origZombieDrop)(void*);
static int16_t hookZombieDrop(void* self) {
	return 0;
}

static void* (*origUseFood)(FoodItem*, void*, void*);
static void* hookUseFood(FoodItem* self, void* ii, void* player) {
	if (self->id == 367) return ii;
	return origUseFood(self, ii, player);
}

static void (*origSetFoodEffect)(FoodItem*, int, int, int, float);
static void hookSetFoodEffect(FoodItem* self, int a, int b, int c, float d) {
	if (self->id == 365) return;
	origSetFoodEffect(self, a, b, c, d);
}

static void* squidPtr;
static void (*origSetAge)(void*, int);
static void hookSetAge(void* self, int age) {
	if (*(void**)self == squidPtr) origSetAge(self, age < 0 ? 0 : age);
	else origSetAge(self, age);
}

static void (*origBiomeDec)(void*, void*, void*, void*, int, std::unique_ptr<OreFeature>&, int, int);
static void hookBiomeDec(void* self, void* tilesrc, void* random, void* tilepos, int a, std::unique_ptr<OreFeature>& feature, int b, int c) {
	OreFeature* f = feature.get();
	if (f->id == 1) f->dmg = 0;
	origBiomeDec(self, tilesrc, random, tilepos, a, feature, b, c);
}

static void (*origAddShapedRecipe)(void*, ItemInstance*, void*, void*, void*);
static void hookAddShapedRecipe(void* self, ItemInstance* output, void* pattern1, void* pattern2, void* ingredients) {
	if (output->valid && output->tile->id == 1 && output->dmg == 3) return;
	origAddShapedRecipe(self, output, pattern1, pattern2, ingredients);
}

static void (*origAddShapedRecipe2)(void*, ItemInstance*, void*, void*);
static void hookAddShapedRecipe2(void* self, ItemInstance* output, void* pattern1, void* ingredients) {
	if (output->valid && output->tile->id == 1 && (output->dmg == 1 || output->dmg == 5)) return;
	origAddShapedRecipe2(self, output, pattern1, ingredients);
}

static bool (*origSetPos)(void*, TilePos*, char vec[24]);
static bool hookSetPos(void* self, TilePos* tp, char vec[24]) {
	if (*(bool*)((char*)self + 0xCFB)) return origSetPos(self, tp, vec);
	tp->y--;
	return true;
}

static std::string* (*getCurLanguage)();
static int (*getItemInstanceId)(ItemInstance*);
static void (*origUpdateResult)(void*, ItemInstance*);
static void (*str_convert)(void*, std::string*);
static void hookUpdateResult(void* self, ItemInstance* ii) {
	origUpdateResult(self, ii);
	ItemInstance* ii_res = (ItemInstance*)((uintptr_t)self + 0xE0);
	int id = getItemInstanceId(ii_res);
	if (!id) return;
	auto langIt = g_descriptions.find(*getCurLanguage());
	if (langIt == g_descriptions.end()) return;
	char strBuf[10];
	if (ii_res->dmg) sprintf(strBuf, "%u:%u", id, ii_res->dmg);
	else sprintf(strBuf, "%u", id);
	auto itemIt = langIt->second.find(std::string(strBuf));
	if (itemIt == langIt->second.end()) return;
	str_convert((void*)((uintptr_t)self + 0xDC), &itemIt->second);
}

static ItemInstance (*ItemInstanceCons)(int id, int count, int dmg);
static void (*addFurnaceRecipe)(void* self, int input_id, ItemInstance* ii_output);
static void* (*origFurnaceCons)(void*);
static void* hookFurnaceCons(void* self) {
	void* result = origFurnaceCons(self);
	ItemInstance ii_coal = ItemInstanceCons(263, 1, 0);
	ItemInstance ii_lapis = ItemInstanceCons(351, 1, 4);
	ItemInstance ii_redstone = ItemInstanceCons(331, 1, 0);
	addFurnaceRecipe(self, 16, &ii_coal);
	addFurnaceRecipe(self, 21, &ii_lapis);
	addFurnaceRecipe(self, 73, &ii_redstone);
	return result;
}

static bool isFlat = false;
static int (*origCreateGenerator)(void*, void*, int);
static int hookCreateGenerator(void* retPtr, void* self, int type) {
	if (type == 2) isFlat = true;
	else isFlat = false;
	return origCreateGenerator(retPtr, self, type);
}

static bool (*origTerrainSave)(void*, int);
static bool hookTerrainSave(void* self, int lastSavedVersion) {
	int* lastModifiedVersion = (int*)((char*)self + 0x14F3C);
	if (((*lastModifiedVersion < -1000000 && !isFlat) || *lastModifiedVersion >= lastSavedVersion) && *(bool*)((char*)self + 0x24) == false) return true;
	return false;
}

static void (*origSaveDirtyChunks)(void*);
static void hookSaveDirtyChunks(void* self) {}

static void (*origChunkSetSaved)(void*);
static void hookChunkSetSaved(void* self) {
	origChunkSetSaved(self);
	if (!isFlat && (uintptr_t)__builtin_return_address(0) - handle->base == (v0_11_1_2 ? 0x17F58D : 0x2C813F)) *(int*)((char*)self + 0x14F3C) = -1000000;
}

static std::vector<void*> tickedRideables;
static void (*origTickWorld)(void*, void*);
static void hookTickWorld(void* self, void* t) {
	tickedRideables.clear();
	origTickWorld(self, t);
}

static void* minecartPtr;
static void* boatPtr;
static void (*origEntityTick)(void*, void*);
static void hookEntityTick(void* self, void* ts) {
	void* vtable = *(void**)self;
	if (vtable == minecartPtr || vtable == boatPtr) {
		for (int i = 0; i < tickedRideables.size(); i++) if (tickedRideables[i] == self) return;
		tickedRideables.push_back(self);
	}
	origEntityTick(self, ts);
}

static void (*updatePlayerSource)(void* self, float* coords, float val);
static void (*origBedUse)(void*, void*, int, int, int);
static void hookBedUse(void* self, void* player, int x, int y, int z) {
	updatePlayerSource(*(void**)((char*)player + 0xCC4), (float*)((char*)player + 24), 3.0f);
	origBedUse(self, player, x, y, z);
}

static void* (*getPlayerByGuid)(void* self, void* guid);
static void (*redistributePacket)(void* self, void* packet, void* guid);
static void (*origServerPlayerMoveHandler)(void*, void*, void*);
static void hookServerPlayerMoveHandler(void* self, void* guid, void* packet) {
	void* player = getPlayerByGuid(self, guid);
	if (player != nullptr) {
		void* ridingOn = *(void**)((char*)player + 0xFC);
		if (ridingOn) {
			*(float*)((char*)player + 0x15C) = *(float*)((char*)packet + 0x2C);
			float newPitch = *(float*)((char*)packet + 0x24);
			if (newPitch != *(float*)((char*)player + 0x58)) {
				*(float*)((char*)player + 3040) = newPitch;
				*(int*)((char*)player + 3048) = 3;
			}
			if (*(void**)ridingOn != boatPtr) {
				float newYaw = *(float*)((char*)packet + 0x28);
				if (newYaw != *(float*)((char*)player + 0x54)) {
					*(float*)((char*)player + 3044) = newYaw;
					*(int*)((char*)player + 3048) = 3;
				}
			}
			redistributePacket(self, packet, guid);
		} else origServerPlayerMoveHandler(self, guid, packet);
	}
}

static void* serverPlayerPtr;
static void (*origInterpolatorTick)(void*, void*);
static void hookInterpolatorTick(void* self, void* entity) {
	void* vtable = *(void**)entity;
	if (vtable == minecartPtr) return;
	if (vtable == serverPlayerPtr && *(bool*)((char*)entity + 0xFC)) {
		memcpy(self, (char*)entity + 24, 12);
	}
	origInterpolatorTick(self, entity);
}

static void* localPlayerPtr;
static void (*origWalkingAnim)(void*);
static void hookWalkingAnim(void* self) {
	void* vtable = *(void**)self;
	if (vtable == localPlayerPtr && *(bool*)((char*)self + 3197)) {
		float* yMot = (float*)((char*)self + 76);
		if (*yMot > 0.0f && *yMot < 0.05f) {
			float yMotSave = *yMot;
			*yMot = 0.0f;
			origWalkingAnim(self);
			*yMot = yMotSave;
			return;
		}
	}
	origWalkingAnim(self);
}

static void (*origServerStart)(void*, int, int);
static void hookServerStart(void* self, int port, int max) {
	origServerStart(self, port, 50);
}

static const char* path = "/storage/emulated/0/games/com.mojang/minecraftpe/better011.txt";
static bool gravel_paths = true;
static bool no_skinpacks = true;
static bool cant_shovelpaths = true;
static bool faster_boats = true;
static bool zombies_nofleshdrop = true;
static bool inedible_flesh = true;
static bool rawchicken_nohunger = true;
static bool no_babysquids = true;
static bool no_ande_dior_gran = true;
static bool no_ande_dior_gran_recipes = true;
static bool entity_lighting_fix = true;
static bool teleportup_fix = true;
static bool furnace_descs = true;
static bool more_smeltable_ores = true;
static bool mushislands_fix = true;
static bool save_all_chunks = true;
static bool no_babyzombies = true;
static bool minecartboat_stutter_fix = true;
static bool bedkick_fix = true;
static bool minecartboat_mp_fix = true;
static bool flyanimation_fix = true;
static int maxplayers = 5;
static int port = 19132;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	handle = (soinfo2*)dlopen("libminecraftpe.so", RTLD_LAZY);
	dlerror();

	FILE* f = fopen(path, "r");
	if (f) {
		fclose(f);
		gravel_paths = ini_getl("", "gravel_paths", 1, path);
		no_skinpacks = ini_getl("", "no_skinpacks", 1, path);
		cant_shovelpaths = ini_getl("", "cant_shovelpaths", 1, path);
		faster_boats = ini_getl("", "faster_boats", 1, path);
		zombies_nofleshdrop = ini_getl("", "zombies_nofleshdrop", 1, path);
		inedible_flesh = ini_getl("", "inedible_flesh", 1, path);
		rawchicken_nohunger = ini_getl("", "rawchicken_nohunger", 1, path);
		no_babysquids = ini_getl("", "no_babysquids", 1, path);
		no_ande_dior_gran = ini_getl("", "no_ande_dior_gran", 1, path);
		no_ande_dior_gran_recipes = ini_getl("", "no_ande_dior_gran_recipes", 1, path);
		entity_lighting_fix = ini_getl("", "entity_lighting_fix", 1, path);
		teleportup_fix = ini_getl("", "teleportup_fix", 1, path);
		furnace_descs = ini_getl("", "furnace_descs", 1, path);
		more_smeltable_ores = ini_getl("", "more_smeltable_ores", 1, path);
		mushislands_fix = ini_getl("", "mushislands_fix", 1, path);
		save_all_chunks = ini_getl("", "save_all_chunks", 1, path);
		no_babyzombies = ini_getl("", "no_babyzombies", 1, path);
		minecartboat_stutter_fix = ini_getl("", "minecartboat_stutter_fix", 1, path);
		bedkick_fix = ini_getl("", "bedkick_fix", 1, path);
		minecartboat_mp_fix = ini_getl("", "minecartboat_mp_fix", 1, path);
		flyanimation_fix = ini_getl("", "flyanimation_fix", 1, path);
		maxplayers = ini_getl("", "maxplayers", 5, path);
		port = ini_getl("", "port", 19132, path);
	} else {
		ini_putl("", "gravel_paths", 1, path);
		ini_putl("", "no_skinpacks", 1, path);
		ini_putl("", "cant_shovelpaths", 1, path);
		ini_putl("", "faster_boats", 1, path);
		ini_putl("", "zombies_nofleshdrop", 1, path);
		ini_putl("", "inedible_flesh", 1, path);
		ini_putl("", "rawchicken_nohunger", 1, path);
		ini_putl("", "no_babysquids", 1, path);
		ini_putl("", "no_ande_dior_gran", 1, path);
		ini_putl("", "no_ande_dior_gran_recipes", 1, path);
		ini_putl("", "entity_lighting_fix", 1, path);
		ini_putl("", "teleportup_fix", 1, path);
		ini_putl("", "furnace_descs", 1, path);
		ini_putl("", "more_smeltable_ores", 1, path);
		ini_putl("", "mushislands_fix", 1, path);
		ini_putl("", "save_all_chunks", 1, path);
		ini_putl("", "no_babyzombies", 1, path);
		ini_putl("", "minecartboat_stutter_fix", 1, path);
		ini_putl("", "bedkick_fix", 1, path);
		ini_putl("", "minecartboat_mp_fix", 1, path);
		ini_putl("", "flyanimation_fix", 1, path);
		ini_putl("", "maxplayers", 5, path);
		ini_putl("", "port", 19132, path);
	}

	squidPtr = (void*)((uintptr_t)dlsym(handle, "_ZTV5Squid") + 8);
	minecartPtr = (void*)((uintptr_t)dlsym(handle, "_ZTV16MinecartRideable") + 8);
	boatPtr = (void*)((uintptr_t)dlsym(handle, "_ZTV4Boat") + 8);
	serverPlayerPtr = (void*)((uintptr_t)dlsym(handle, "_ZTV12ServerPlayer") + 8);
	localPlayerPtr = (void*)((uintptr_t)dlsym(handle, "_ZTV11LocalPlayer") + 8);

	if ((uintptr_t)squidPtr - handle->base == 0x407678) v0_11_1_2 = true;
	else v0_11_1_1 = true;

	getCurLanguage = (std::string*(*)())(dlsym(handle, "_ZN4I18n18getCurrentLanguageEv"));
	getItemInstanceId = (int(*)(ItemInstance*))(dlsym(handle, "_ZNK12ItemInstance5getIdEv"));
	ItemInstanceCons = (ItemInstance(*)(int, int, int))(dlsym(handle, "_ZN12ItemInstanceC1Eiii"));
	addFurnaceRecipe = (void(*)(void*, int, ItemInstance*))(dlsym(handle, "_ZN14FurnaceRecipes16addFurnaceRecipeEiRK12ItemInstance"));
	updatePlayerSource = (void(*)(void*, float*, float))(dlsym(handle, "_ZN17PlayerChunkSource8centerAtERK4Vec3f"));
	getPlayerByGuid = (void*(*)(void*, void*))(dlsym(handle, "_ZN20ServerNetworkHandler9getPlayerERKN6RakNet10RakNetGUIDE"));
	redistributePacket = (void(*)(void*, void*, void*))(dlsym(handle, "_ZN20ServerNetworkHandler18redistributePacketEP6PacketRKN6RakNet10RakNetGUIDE"));

	if (gravel_paths) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN12VillagePiece10biomeBlockE8FullTile") | 1),
					(void*)hookPlaceBlock, (void**)&origPlaceBlock);

	if (no_skinpacks) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN14SkinRepositoryC1ER9GameStoreR8TexturesRKSs") | 1),
					(void*)hookSkinRepo, (void**)&origSkinRepo);

	if (cant_shovelpaths) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN10ShovelItem5useOnEP12ItemInstanceP6Playeriiiafff") | 1),
					(void*)hookShovelUseOn, (void**)&origShovelUseOn);

	if (faster_boats) {
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN4Boat12_paddleForceE4Side") | 1),
					(void*)hookPaddleForce, (void**)&origPaddleForce);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN4Boat14_addPaddleTimeE4Sidef") | 1),
					(void*)hookAddPaddleTime, (void**)&origAddPaddleTime);
	}

	if (zombies_nofleshdrop) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN6Zombie12getDeathLootEv") | 1),
					(void*)hookZombieDrop, (void**)&origZombieDrop);

	if (inedible_flesh) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN8FoodItem3useER12ItemInstanceR6Player") | 1),
					(void*)hookUseFood, (void**)&origUseFood);

	if (rawchicken_nohunger) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN8FoodItem12setEatEffectEiiif") | 1),
					(void*)hookSetFoodEffect, (void**)&origSetFoodEffect);

	if (no_babysquids) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN9AgableMob6setAgeEi") | 1),
					(void*)hookSetAge, (void**)&origSetAge);

	if (no_ande_dior_gran) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN14BiomeDecorator17decorateDepthSpanEP10TileSourceR6RandomRK7TilePosiRSt10unique_ptrI7FeatureSt14default_deleteIS8_EEii") | 1),
					(void*)hookBiomeDec, (void**)&origBiomeDec);

	if (no_ande_dior_gran_recipes) {
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN7Recipes15addShapedRecipeERK12ItemInstanceRKSsS4_RKSt6vectorINS_4TypeESaIS6_EE") | 1),
					(void*)hookAddShapedRecipe, (void**)&origAddShapedRecipe);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN7Recipes15addShapedRecipeERK12ItemInstanceRKSsRKSt6vectorINS_4TypeESaIS6_EE") | 1),
					(void*)hookAddShapedRecipe2, (void**)&origAddShapedRecipe2);
	}

	if (entity_lighting_fix) {
		uintptr_t addr = (v0_11_1_2 ? 0x1BCB84: 0x2CACEC) + handle->base;
		uintptr_t pageStart = addr & ~0xFFF;
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
		*(uint16_t*)addr = 0xE002;
		__builtin___clear_cache((char*)addr, (char*)addr + 2);
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_EXEC);
	}

	if (teleportup_fix) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZNK6Player16fixSpawnPositionER7TilePosSt6vectorIP10TileSourceSaIS4_EE") | 1),
					(void*)hookSetPos, (void**)&origSetPos);

	if (furnace_descs) {
		InitDescriptions();
		uintptr_t addr = v0_11_1_2 ? 0x37e699 : 0x3ab8dc;
		str_convert = (void(*)(void*, std::string*))(addr + handle->base);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN13FurnaceScreen12updateResultEPK12ItemInstance") | 1),
					(void*)hookUpdateResult, (void**)&origUpdateResult);
	}

	if (more_smeltable_ores) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN14FurnaceRecipesC2Ev") | 1),
					(void*)hookFurnaceCons, (void**)&origFurnaceCons);

	if (mushislands_fix) {
		uintptr_t addr = (v0_11_1_2 ? 0x1869FE: 0x296168) + handle->base;
		uintptr_t pageStart = addr & ~0xFFF;
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
		*((uint8_t*)addr + 1) = 0xDD;
		__builtin___clear_cache((char*)addr, (char*)addr + 2);
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_EXEC);
	}

	if (save_all_chunks) {
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN5Level16_createGeneratorE13GeneratorType") | 1),
					(void*)hookCreateGenerator, (void**)&origCreateGenerator);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZNK10LevelChunk16needsTerrainSaveEi") | 1),
					(void*)hookTerrainSave, (void**)&origTerrainSave);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN5Level20_saveSomeDirtyChunksEv") | 1),
					(void*)hookSaveDirtyChunks, (void**)&origSaveDirtyChunks);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN10LevelChunk8setSavedEv") | 1),
					(void*)hookChunkSetSaved, (void**)&origChunkSetSaved);
	}

	if (no_babyzombies) {
		uintptr_t addr = (v0_11_1_2 ? 0x2D94D0: 0x2905D0) + handle->base;
		uintptr_t pageStart = addr & ~0xFFF;
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
		*(float*)addr = 0.0f;
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_EXEC);
	}

	if (minecartboat_stutter_fix) {
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN6Player9tickWorldERK4Tick") | 1),
					(void*)hookTickWorld, (void**)&origTickWorld);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN6Entity4tickER10TileSource") | 1),
					(void*)hookEntityTick, (void**)&origEntityTick);
	}

	if (bedkick_fix) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN7BedTile3useEP6Playeriii") | 1),
					(void*)hookBedUse, (void**)&origBedUse);

	if (minecartboat_mp_fix) {
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN20ServerNetworkHandler6handleERKN6RakNet10RakNetGUIDEP16MovePlayerPacket") | 1),
					(void*)hookServerPlayerMoveHandler, (void**)&origServerPlayerMoveHandler);
		MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN20MovementInterpolator4tickER6Entity") | 1),
					(void*)hookInterpolatorTick, (void**)&origInterpolatorTick);
		uintptr_t addr = (v0_11_1_2 ? 0x2E520C: 0x179958) + handle->base;
		uintptr_t pageStart = addr & ~0xFFF;
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
		*((uint16_t*)addr) = 0x2300;
		__builtin___clear_cache((char*)addr, (char*)addr + 2);
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_EXEC);
	}

	if (flyanimation_fix) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN3Mob14updateWalkAnimEv") | 1),
					(void*)hookWalkingAnim, (void**)&origWalkingAnim);

	if (maxplayers != 5 || port != 19132) MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN9Minecraft15hostMultiplayerEii") | 1),
					(void*)hookServerStart, (void**)&origServerStart);

	const char* error = dlerror();
	if (error) __android_log_print(ANDROID_LOG_INFO, "Better 0.11", "dlerror: %s", error);
	return JNI_VERSION_1_2;
}
