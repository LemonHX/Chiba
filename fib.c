
#include "include/utils/chiba_utils.h"

C8ReflStruct(hi2, (i, i32), (j, u64));

void clean_hi2(CHIBA_hi2 *obj) {
  printf("Cleaning up hi2 struct with i=%d, j=%llu\n", obj->i, obj->j);
}

void print_dyn_ty(CHIBA_DYN_TY val) {
  if (strcmp(val.type_name, "i32") == 0) {
    i32 v = *(i32 *)val.data;
    printf("i32 value: %d\n", v);
  } else if (strcmp(val.type_name, "u64") == 0) {
    u64 v = *(u64 *)val.data;
    printf("u64 value: %llu\n", v);
  } else {
    printf("Unknown type: %s\n", val.type_name);
  }
}

i32 main() {
  C8(hi2) DEFER(clean_hi2) obj = _MK_CHIBA_hi2((C8(hi2)){.i = 42, .j = 100});

  char buffer[128];
  printf("Enter field name to get (i or j): ");
  scanf("%s", buffer);
  str field_name = (str){.data = (i8 *)buffer, .len = strlen(buffer)};
  CHIBA_DYN_TY field_value =
      CHIBA_dyn_get_field((CHIBA_METAINFO_HEADER *)&obj, field_name);
  print_dyn_ty(field_value);
}
