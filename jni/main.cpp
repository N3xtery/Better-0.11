#include <jni.h>
#include <dlfcn.h>
#include "Substrate.h"
#include <android/log.h>
#include <memory>
#include "main.h"
#include <sys/mman.h>

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

struct Player {
	char buf[0xCFB];
	bool loaded;
};

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
	if (f->id == 1) return;
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

static void (*TilePosFromVec3)(void* out, void* vec3);
static uint32_t (*getLightColor)(void* tileSource, void* tilePos, int minBrightness);
static void (*getColorForUV)(void* out, uint32_t uv);
static void (*shaderUniformSet)(void* uniform, void* color);
static void (*origSetBrightness)(void*, void*, void*, float);
static void hookSetBrightness(void* self, void* entity, void* color, float partialTick) {
	float tilePos[3];
	tilePos[0] = *(float*)((uint8_t*)entity + 0x18);
	tilePos[1] = *(float*)((uint8_t*)entity + 0x1C);
	tilePos[2] = *(float*)((uint8_t*)entity + 0x20);

	int tp[3];
	TilePosFromVec3(tp, tilePos);

	void* tileSource = *(void**)((uint8_t*)entity + 0x30);
	uint32_t uv = getLightColor(tileSource, tp, 0);

	float lightColor[4];
	getColorForUV(lightColor, uv);

	shaderUniformSet(*(void**)((uint8_t*)self + 0x28), lightColor);
	shaderUniformSet(*(void**)((uint8_t*)self + 0x2C), color);
}

static bool (*origSetPos)(Player*, TilePos*, char vec[24]);
static bool hookSetPos(Player* self, TilePos* tp, char vec[24]) {
	if (self->loaded) return origSetPos(self, tp, vec);
	tp->y--;
	return true;
}

static std::string* (*getCurLanguage)();
static void* (*origGetStr)(void*, const std::string*, void* vec);
static void* hookGetStr(void* outStr, const std::string* key, void* vec) {
	uintptr_t retAddr = (uintptr_t)__builtin_return_address(0) - handle->base;
	if (retAddr >= 0x1F341C && retAddr <= 0x1F35FA) {
		auto langIt = g_descriptions.find(*getCurLanguage());
		if (langIt == g_descriptions.end()) return origGetStr(outStr, key, vec);
		auto itemIt = langIt->second.find(*key);
		if (itemIt == langIt->second.end()) return origGetStr(outStr, key, vec);
		((void(*)(void*, std::string*))(handle->base+0x37E698|1))(outStr, &itemIt->second);
		return outStr;
	}
	return origGetStr(outStr, key, vec);
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

static void PatchDecorateGuard(uintptr_t baseAddr) {
	uintptr_t addr = baseAddr + 0x1869FE;
	uintptr_t pageStart = addr & ~0xFFF;
	mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
	uint8_t* instr = (uint8_t*)addr;
	if (instr[1] != 0xD0) {
		mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_EXEC);
		return;
	}
	instr[1] = 0xDD;
	__builtin___clear_cache((char*)addr, (char*)addr + 2);
	mprotect((void*)pageStart, 0x2000, PROT_READ | PROT_EXEC);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	handle = (soinfo2*)dlopen("libminecraftpe.so", RTLD_LAZY);
	dlerror();

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN12VillagePiece10biomeBlockE8FullTile") | 1),
				   (void*)hookPlaceBlock, (void**)&origPlaceBlock);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN14SkinRepositoryC1ER9GameStoreR8TexturesRKSs") | 1),
				   (void*)hookSkinRepo, (void**)&origSkinRepo);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN10ShovelItem5useOnEP12ItemInstanceP6Playeriiiafff") | 1),
				   (void*)hookShovelUseOn, (void**)&origShovelUseOn);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN4Boat12_paddleForceE4Side") | 1),
				   (void*)hookPaddleForce, (void**)&origPaddleForce);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN4Boat14_addPaddleTimeE4Sidef") | 1),
				   (void*)hookAddPaddleTime, (void**)&origAddPaddleTime);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN6Zombie12getDeathLootEv") | 1),
				   (void*)hookZombieDrop, (void**)&origZombieDrop);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN8FoodItem3useER12ItemInstanceR6Player") | 1),
				   (void*)hookUseFood, (void**)&origUseFood);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN8FoodItem12setEatEffectEiiif") | 1),
				   (void*)hookSetFoodEffect, (void**)&origSetFoodEffect);

	squidPtr = (void*)((uintptr_t)dlsym(handle, "_ZTV5Squid") + 8);
	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN9AgableMob6setAgeEi") | 1),
				   (void*)hookSetAge, (void**)&origSetAge);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN14BiomeDecorator17decorateDepthSpanEP10TileSourceR6RandomRK7TilePosiRSt10unique_ptrI7FeatureSt14default_deleteIS8_EEii") | 1),
				   (void*)hookBiomeDec, (void**)&origBiomeDec);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN7Recipes15addShapedRecipeERK12ItemInstanceRKSsS4_RKSt6vectorINS_4TypeESaIS6_EE") | 1),
				   (void*)hookAddShapedRecipe, (void**)&origAddShapedRecipe);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN7Recipes15addShapedRecipeERK12ItemInstanceRKSsRKSt6vectorINS_4TypeESaIS6_EE") | 1),
				   (void*)hookAddShapedRecipe2, (void**)&origAddShapedRecipe2);

	TilePosFromVec3 = (void(*)(void*, void*))(dlsym(handle, "_ZN7TilePosC1ERK4Vec3"));
	getLightColor = (uint32_t(*)(void*, void*, int))(dlsym(handle, "_ZN10TileSource13getLightColorERK7TilePosi"));
	getColorForUV = (void(*)(void*, uint32_t))(dlsym(handle, "_ZN12LightTexture13getColorForUVEj"));
	shaderUniformSet = (void(*)(void*, void*))(dlsym(handle, "_ZN13ShaderUniformlsERK5Color"));
	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN19EntityShaderManager22_setupShaderParametersER6EntityRK5Colorf") | 1),
				   (void*)hookSetBrightness, (void**)&origSetBrightness);

	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZNK6Player16fixSpawnPositionER7TilePosSt6vectorIP10TileSourceSaIS4_EE") | 1),
				   (void*)hookSetPos, (void**)&origSetPos);

	InitDescriptions();
	getCurLanguage = (std::string*(*)())(dlsym(handle, "_ZN4I18n18getCurrentLanguageEv"));
	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN4I18n3getERKSsRKSt6vectorISsSaISsEE") | 1),
				   (void*)hookGetStr, (void**)&origGetStr);

	ItemInstanceCons = (ItemInstance(*)(int, int, int))(dlsym(handle, "_ZN12ItemInstanceC1Eiii"));
	addFurnaceRecipe = (void(*)(void*, int, ItemInstance*))(dlsym(handle, "_ZN14FurnaceRecipes16addFurnaceRecipeEiRK12ItemInstance"));
	MSHookFunction((void*)((uintptr_t)dlsym(handle, "_ZN14FurnaceRecipesC2Ev") | 1),
				   (void*)hookFurnaceCons, (void**)&origFurnaceCons);

	PatchDecorateGuard(handle->base);

	const char* error = dlerror();
	if (error) __android_log_print(ANDROID_LOG_INFO, "Better 0.11", "dlerror: %s", error);
	return JNI_VERSION_1_2;
}
