
#include "include/utils/chiba_utils.h"

C8ReflStruct(hi2, (i, i32), (j, u64), (k, str));

void clean_hi2(CHIBA_hi2 *obj) {
  printf("Cleaning up hi2 struct with i=%d, j=%llu, k=%s\n", obj->i, obj->j, obj->k.data);
}

void print_dyn_ty(C8NS(AnyObjectType) val) {
  if (strcmp(val.type_name, "i32") == 0) {
    i32 v = *(i32 *)val.data;
    printf("i32 value: %d\n", v);
  } else if (strcmp(val.type_name, "u64") == 0) {
    u64 v = *(u64 *)val.data;
    printf("u64 value: %llu\n", v);
  } else if (strcmp(val.type_name, "str") == 0) {
    str v = *(str *)val.data;
    printf("str value: %s\n", v.data);
  } else {
    printf("Unknown type: %s\n", val.type_name);
  }
}

i32 main() {
  C8NS(hi2)
  DEFER(clean_hi2) obj = _MK_CHIBA_hi2((C8NS(hi2)){.i = 42, .j = 100, .k = STR("Hello, Chiba!")});

  char buffer[128];
  printf("Enter field name to get (i j or k): ");
  scanf("%s", buffer);
  C8NS(AnyObjectType) field_value =
      CHIBA_dyn_get_field((C8NS(AnyObject) *)&obj, ToSTR(buffer));
  print_dyn_ty(field_value);
}
