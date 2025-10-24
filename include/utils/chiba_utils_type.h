#pragma once

#define C8(name) CHIBA_##name
#define _C8S(name) CHIBA_##name##_struct
#define Self(name) struct _C8S(name)* self;

#define Field(name, type) type name

// C8Struct(hi, {
//     i32 i;
// });
#define C8Struct(name, body) \
struct _C8S(name); \
typedef struct body C8(name);

// C8AttrStruct(P, (packed, aligned(64)), {
//     i32 i;
//     Self(P)
// });
#define C8AttrStruct(name, attr, body) \
struct _C8S(name); \
typedef struct __attribute__ (attr) _C8S(name) body C8(name);


#include "chiba_utils_basic_types.h"
