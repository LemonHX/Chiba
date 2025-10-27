#pragma once
#include "../utils/chiba_object_hashtable.h"
#include "basic_type.h"

typedef C8STRUCT C8NS(ReflFieldMetaInfo) {
  i8 *const name;
  i8 *const type;
  u32 const size;
  u64 const offset;
} C8NS(ReflFieldMetaInfo);

C8STRUCT C8NS(AnyObject);

// used for reflection metadata
typedef C8STRUCT C8NS(ReflMetaInfo) {
  cstr type_name;
  // not using slice to avoid complexity in ABI
  C8NS(ReflFieldMetaInfo) *const fields;
  u64 const field_count;

  VHashTable *const dyn_vtable;
  C8NS(AnyObject) *const parent_ref;
} C8NS(ReflMetaInfo);

typedef C8STRUCT C8NS(AnyObject) {
  const C8NS(ReflMetaInfo) * metainfo;
} C8NS(AnyObject);

typedef C8STRUCT C8NS(AnyObjectSlice) {
  C8NS(AnyObject) * items;
  u64 count;
} C8NS(AnyObjectSlice);
