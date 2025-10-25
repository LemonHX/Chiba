#pragma once
#include "chiba_utils_basic_types.h"
#include "chiba_utils_math.h"
#include "chiba_utils_refl_vhashtable.h"
#include "chiba_utils_refl_vvec.h"
#include "chiba_utils_visibility.h"

// used for reflection metadata
typedef struct CHIBA_FIELD_METAINFO {
  const i8 *name;
  const i8 *type;
  const u32 size;
  const u64 offset;
} CHIBA_FIELD_METAINFO;

// used for reflection metadata
typedef struct CHIBA_METAINFO {
  const CHIBA_FIELD_METAINFO *fields;
  const u32 field_count;
  const VVec *vtable;
  const VHashTable *field_name_to_index;
} CHIBA_METAINFO;

typedef struct CHIBA_METAINFO_HEADER {
  const CHIBA_METAINFO *metainfo;
} CHIBA_METAINFO_HEADER;

typedef struct CHIBA_DYN_TY {
  i8 *type_name;
  void *data;
} CHIBA_DYN_TY;

PUBLIC CHIBA_DYN_TY CHIBA_dyn_get_field(CHIBA_METAINFO_HEADER *self, str s) {
  for (u64 i = 0; i < self->metainfo->field_count; i++) {
    if (str_equals(
            s, (str){.data = (i8 *)self->metainfo->fields[i].name,
                     .len = strlen((i8 *)self->metainfo->fields[i].name)})) {
      u64 offset = self->metainfo->fields[i].offset;
      return (CHIBA_DYN_TY){
          .type_name = (i8 *)self->metainfo->fields[i].type,
          .data = (void *)((u8 *)self + offset),
      };
    }
  }
  return (CHIBA_DYN_TY){.type_name = NULL, .data = NULL};
}

PUBLIC bool CHIBA_dyn_set_field(CHIBA_METAINFO_HEADER *self, str s,
                                CHIBA_DYN_TY val) {
  for (u64 i = 0; i < self->metainfo->field_count; i++) {
    if (str_equals(
            s, (str){.data = (i8 *)self->metainfo->fields[i].name,
                     .len = strlen((i8 *)self->metainfo->fields[i].name)})) {
      u64 offset = self->metainfo->fields[i].offset;
      if (strcmp((i8 *)self->metainfo->fields[i].type, val.type_name) == 0) {
        memcpy((u8 *)self + offset, val.data, self->metainfo->fields[i].size);
        return true;
      }
    }
  }
  return false;
}

#include "chiba_utils_refl_impl.h"

// generate struct with reflection metadata
// it will generate a pare of `MK_CHIBA_name` for stack and `NEW_CHIBA_name` for
// heap allocation
// ```c
// C8ReflStruct(hi2, (i, i32), (j, u64));
// C8ReflStruct(hi3, (i, i32), (j, u64), (k, f32), (l, f64), (m, C8(hi2)));
//
// void f() {
//   C8(hi3)
//   a = MK_CHIBA_hi3({.i = 10,
//                     .j = 20,
//                     .k = 3.14f,
//                     .l = 2.71828,
//                     .m = MK_CHIBA_hi2({.i = 42})});
//  C8(hi3) *p = NEW_CHIBA_hi3(a, malloc);
// }
// ```
#define C8ReflStruct(struct_name, ...)                                         \
  DECLARE_CHIBA_STRUCT(struct_name, __VA_ARGS__)
