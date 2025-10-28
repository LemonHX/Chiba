#pragma once
#include "../utils/chiba_object_hashtable.h"
#include "basic_type.h"

typedef C8STRUCT C8NS(ReflFieldMetaInfo) {
  i8 *const name;
  i8 *const type;
  u32 const size;
  u64 const offset;
}
C8NS(ReflFieldMetaInfo);

C8STRUCT C8NS(AnyObject);

// used for reflection metadata
typedef C8STRUCT C8NS(ObjectHeader) {
  cstr type_name;   // 0
  Symbol type_hash; // 8

  // not using slice to avoid complexity in ABI
  C8NS(ReflFieldMetaInfo) *const fields; // 16
  u64 const field_count;                 // 24

  VHashTable *const dyn_vtable;      // 32
  C8NS(AnyObject) *const parent_ref; // 40
} // 48
C8NS(ObjectHeader);
static_assert(sizeof(C8NS(ObjectHeader)) == 48, "ObjectHeader size mismatch");

// only use as ptr
typedef C8STRUCT C8NS(AnyObject) {
  const C8NS(ObjectHeader) * metainfo;
  // the object data goes always after metainfo
}
C8NS(AnyObject);
static_assert(sizeof(C8NS(AnyObject)) == 8, "AnyObject size mismatch");

typedef C8STRUCT C8NS(AnyObjectRef) {
  C8NS(AnyObject) * obj;
  anyptr *global_gc_stat;
}
C8NS(AnyObjectRef);

typedef C8STRUCT C8NS(AnyObjectSlice) {
  C8NS(AnyObject) * items;
  u64 count;
}
C8NS(AnyObjectSlice);
static_assert(sizeof(C8NS(AnyObjectSlice)) == 16,
              "AnyObjectSlice size mismatch");
