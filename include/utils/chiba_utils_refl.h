#pragma once
#include "chiba_utils_basic_types.h"
#include "chiba_utils_math.h"
#include "chiba_utils_refl_vhashtable.h"
#include "chiba_utils_visibility.h"

// used for reflection metadata
typedef struct C8NS(ReflFieldMetaInfo) {
  const i8 *name;
  const i8 *type;
  const u32 size;
  const u64 offset;
} C8NS(ReflFieldMetaInfo);

// used for reflection metadata
typedef struct C8NS(ReflMetaInfo) {
  const C8NS(ReflFieldMetaInfo) *fields;
  const u32 field_count;
  const VHashTable *dyn_vtable;
} C8NS(ReflMetaInfo);

typedef struct __attribute__((aligned(8))) C8NS(AnyObject) {
  const C8NS(ReflMetaInfo) *metainfo;
} C8NS(AnyObject);

typedef struct C8NS(AnyObjectType) {
  i8 *type_name;
  void *data;
} C8NS(AnyObjectType);

UTILS C8NS(AnyObjectType) CHIBA_dyn_get_field(C8NS(AnyObject) *self, str s) {
  for (u64 i = 0; i < self->metainfo->field_count; i++) {
    if (str_equals(
            s, (str){.data = (i8 *)self->metainfo->fields[i].name,
                     .len = strlen((i8 *)self->metainfo->fields[i].name)})) {
      u64 offset = self->metainfo->fields[i].offset;
      return (C8NS(AnyObjectType)){
          .type_name = (i8 *)self->metainfo->fields[i].type,
          .data = (void *)((u8 *)self + offset),
      };
    }
  }
  return (C8NS(AnyObjectType)){.type_name = NULL, .data = NULL};
}

UTILS bool CHIBA_dyn_set_field(C8NS(AnyObject) *self, str s,
                                C8NS(AnyObjectType) val) {
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
