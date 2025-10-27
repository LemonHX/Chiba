#pragma once
#include "chiba_utils_visibility.h"
#include "chiba_utils_math.h"
#include "chiba_utils_basic_types.h"

// used for reflection metadata
typedef struct C8NS(ReflFieldMetaInfo) {
    const i8* name;
    const i8* type;
    const u32 size;
} C8NS(ReflFieldMetaInfo);

// used for reflection metadata
typedef struct C8NS(ReflMetaInfo) {
    const C8NS(ReflFieldMetaInfo)* fields;
    const u32 field_count;
} C8NS(ReflMetaInfo);


#define C8(name) CHIBA_##name
#define _C8S(name) CHIBA_##name##_struct
#define Self(name) struct _C8S(name)* self;

#define Field(name, type) type name;

// C8Struct(hi, {
//     Field(i, i32)
//     Field(j, u64)
// });
#define C8Struct(name, body) \
struct _C8S(name); \
typedef struct _C8S(name) body C8(name);

// C8AttrStruct(P, (packed, aligned(64)), {
//     Field(i, i32)
//     Field(j, u64)
// });
#define C8AttrStruct(name, attr, body) \
struct _C8S(name); \
typedef struct __attribute__ (attr) _C8S(name) body C8(name);




struct CHIBA_hi_struct;
typedef struct CHIBA_hi_struct {
    const C8NS(ReflMetaInfo)* metainfo;
    struct CHIBA_hi_struct* self;

    // Field(i, i32)
    int i;
} CHIBA_hi;

const C8NS(ReflFieldMetaInfo) CHIBA_hi_FIELD_METAINFO[] = {
    // Field(i, i32)
    {.name = "hi", .type = "i32", .size = sizeof(i32)},
};

const C8NS(ReflMetaInfo) CHIBA_hi_METAINFO = {
    .fields = CHIBA_hi_FIELD_METAINFO,
    .field_count = countof(CHIBA_hi_FIELD_METAINFO),
};

UTILS CHIBA_hi MK_CHIBA_hi(CHIBA_hi val) {
    val.metainfo = &CHIBA_hi_METAINFO;
    return val;
}

UTILS CHIBA_hi* NEW_CHIBA_hi(CHIBA_hi val, void*(*alloc_f)(u64)) {
    CHIBA_hi new_val = MK_CHIBA_hi(val);
    CHIBA_hi* p = (CHIBA_hi*)alloc_f(sizeof(CHIBA_hi));
    memcpy(p, &new_val, sizeof(CHIBA_hi));
    return p;
}
