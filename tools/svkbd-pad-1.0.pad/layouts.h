#define keys keys_en
#define buttonmods buttonmods_en
#include "layout.en.h"
#undef keys
#undef buttonmods
#undef sys_switch
#undef NO_REPEAT

#define keys keys_en_fnx
#define buttonmods buttonmods_en_fnx
#include "layout.en.fnx.h"
#undef keys
#undef buttonmods
#undef sys_switch
#undef NO_REPEAT

#define keys keys_en_full
#define buttonmods buttonmods_en_full
#include "layout.en.full.h"
#undef keys
#undef buttonmods
#undef sys_switch
#undef NO_REPEAT

#define keys keys_ru
#define buttonmods buttonmods_ru
#include "layout.ru.h"
#undef keys
#undef buttonmods
#undef sys_switch
#undef NO_REPEAT

#define keys keys_ru_fnx
#define buttonmods buttonmods_ru_fnx
#include "layout.ru.fnx.h"
#undef keys
#undef buttonmods
#undef sys_switch
#undef NO_REPEAT

#define keys keys_ru_full
#define buttonmods buttonmods_ru_full
#include "layout.ru.full.h"
#undef keys
#undef buttonmods
#undef sys_switch
#undef NO_REPEAT

#define keys keys_cursor_v_side
#define buttonmods buttonmods_cursor_v_side
#include "layout.cursor.v.side.h"
#undef keys
#undef buttonmods
#undef sys_switch
#undef NO_REPEAT

typedef struct {
	const char *name;
	const char *shortname;
	int group;
	const char *variant;
	Key *keys;
	int nkeys;
	Buttonmod *buttonmods;
	int nbuttonmods;
} Layout;

static const Layout layouts[] = {
	{ "en", "●", 0, "", keys_en, LENGTH(keys_en), buttonmods_en, LENGTH(buttonmods_en) },
	{ "en.fnx", "●", 0, "fnx", keys_en_fnx, LENGTH(keys_en_fnx), buttonmods_en_fnx, LENGTH(buttonmods_en_fnx) },
	{ "en.full", "●", 0, "full", keys_en_full, LENGTH(keys_en_full), buttonmods_en_full, LENGTH(buttonmods_en_full) },
	{ "ru", "●", 1, "", keys_ru, LENGTH(keys_ru), buttonmods_ru, LENGTH(buttonmods_ru) },
	{ "ru.fnx", "●", 1, "fnx", keys_ru_fnx, LENGTH(keys_ru_fnx), buttonmods_ru_fnx, LENGTH(buttonmods_ru_fnx) },
	{ "ru.full", "●", 1, "full", keys_ru_full, LENGTH(keys_ru_full), buttonmods_ru_full, LENGTH(buttonmods_ru_full) },
	{ "cursor.v.side", "●", 0, "cursor.v.side", keys_cursor_v_side, LENGTH(keys_cursor_v_side), buttonmods_cursor_v_side, LENGTH(buttonmods_cursor_v_side) },
};

#define NLAYOUTS LENGTH(layouts)
