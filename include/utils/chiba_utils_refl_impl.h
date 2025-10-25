#pragma once
#include "chiba_utils_basic_types.h"
#include "chiba_utils_math.h"
#include "chiba_utils_visibility.h"

#define DECLARE_CHIBA_STRUCT_1(struct_name, field1)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


#define DECLARE_CHIBA_STRUCT_2(struct_name, field1, field2)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1 EXPAND_FIELD field2                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field2)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


#define DECLARE_CHIBA_STRUCT_3(struct_name, field1, field2, field3)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1 EXPAND_FIELD field2 EXPAND_FIELD field3                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field2) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field3)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


#define DECLARE_CHIBA_STRUCT_4(struct_name, field1, field2, field3, field4)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1 EXPAND_FIELD field2 EXPAND_FIELD field3 EXPAND_FIELD field4                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field2) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field3) \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field4)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


#define DECLARE_CHIBA_STRUCT_5(struct_name, field1, field2, field3, field4, field5)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1 EXPAND_FIELD field2 EXPAND_FIELD field3 EXPAND_FIELD field4 EXPAND_FIELD field5                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field2) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field3) \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field4) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field5)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


#define DECLARE_CHIBA_STRUCT_6(struct_name, field1, field2, field3, field4, field5, field6)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1 EXPAND_FIELD field2 EXPAND_FIELD field3 EXPAND_FIELD field4 EXPAND_FIELD field5 EXPAND_FIELD field6                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field2) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field3) \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field4) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field5) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field6)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


#define DECLARE_CHIBA_STRUCT_7(struct_name, field1, field2, field3, field4, field5, field6, field7)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1 EXPAND_FIELD field2 EXPAND_FIELD field3 EXPAND_FIELD field4 EXPAND_FIELD field5 EXPAND_FIELD field6 EXPAND_FIELD field7                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field2) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field3) \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field4) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field5) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field6) \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field7)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


#define DECLARE_CHIBA_STRUCT_8(struct_name, field1, field2, field3, field4, field5, field6, field7, field8)  \
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \
  typedef struct CHIBA_##struct_name##_struct {                                \
    const CHIBA_METAINFO *metainfo;                                            \
    EXPAND_FIELD field1 EXPAND_FIELD field2 EXPAND_FIELD field3 EXPAND_FIELD field4 EXPAND_FIELD field5 EXPAND_FIELD field6 EXPAND_FIELD field7 EXPAND_FIELD field8                \
  } CHIBA_##struct_name;                                                       \
  const CHIBA_FIELD_METAINFO CHIBA_##struct_name##_FIELD_METAINFO[] = {        \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field1) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field2) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field3) \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field4) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field5) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field6) \
      EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field7) EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field8)};              \
  DECLARE_CHIBA_METADATA(struct_name);                                         \
  PUBLIC static VVec *CHIBA_##struct_name##_vtable; \
  PUBLIC static VHashTable *CHIBA_##struct_name##_field_name_to_index; \
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) { \
  CHIBA_##struct_name##_vtable = vvec_create(); \
  CHIBA_##struct_name##_field_name_to_index = vhashtable_create(); \
  } \
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)


// 辅助宏
#define DECLARE_CHIBA_STRUCT_FORWARD(struct_name)                              \
  struct CHIBA_##struct_name##_struct;

#define EXPAND_FIELD(field_name, field_type) field_type field_name;
#define EXPAND_FIELD_METADATA_WITH_OFFSET(struct_name, field)                                 \
{.offset = offsetof(struct_name, EXPAND_FIELD_EXTRACT_FIELD_NAME field), EXPAND_FIELD_METADATA field
#define EXPAND_FIELD_EXTRACT_FIELD_NAME(field_name, field_type) field_name
#define EXPAND_FIELD_METADATA(field_name, field_type)                          \
 .name = #field_name, .type = #field_type, .size = sizeof(field_type)},

#define DECLARE_CHIBA_METADATA(struct_name)                                    \
  const CHIBA_METAINFO CHIBA_##struct_name##_METAINFO = {                      \
      .fields = CHIBA_##struct_name##_FIELD_METAINFO,                          \
      .field_count = countof(CHIBA_##struct_name##_FIELD_METAINFO),            \
  };

#define DECLARE_CHIBA_CONSTRUCTORS(struct_name)                                \
  UTILS CHIBA_##struct_name _MK_CHIBA_##struct_name(CHIBA_##struct_name val) {  \
    val.metainfo = &CHIBA_##struct_name##_METAINFO;                            \
    return val;                                                                \
  }                                                                            \
  UTILS CHIBA_##struct_name *_NEW_CHIBA_##struct_name(CHIBA_##struct_name val,  \
                                                     void *(*alloc_f)(u64)) {  \
    CHIBA_##struct_name new_val = _MK_CHIBA_##struct_name(val);                 \
    CHIBA_##struct_name *p =                                                   \
        (CHIBA_##struct_name *)alloc_f(sizeof(CHIBA_##struct_name));           \
    *p = new_val;                                                              \
    return p;                                                                  \
  }

// 主宏 - 根据参数数量选择正确的版本
#define DECLARE_CHIBA_STRUCT(struct_name, ...)                                 \
  MACRO_SELECT(__VA_ARGS__, DECLARE_CHIBA_STRUCT_8, DECLARE_CHIBA_STRUCT_7, DECLARE_CHIBA_STRUCT_6, DECLARE_CHIBA_STRUCT_5, DECLARE_CHIBA_STRUCT_4, DECLARE_CHIBA_STRUCT_3, DECLARE_CHIBA_STRUCT_2, DECLARE_CHIBA_STRUCT_1)(struct_name, __VA_ARGS__)

#define MACRO_SELECT(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME
