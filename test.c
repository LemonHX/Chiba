#include "chiba/basic_types.h"

int main() {
  cstr s = "Hello, Chiba!";
  printf("String: %s, Address: %p\n", s, (anyptr)s);

  u64 boxed = CHIBA_ptr_to_nanbox((anyptr)s);
  printf("Boxed u64: 0x%016llx\n", boxed);

  CHIBA_NanBox box;
  box.abi = boxed;
  printf("Box breakdown:\n");
  printf("  payload: 0x%012llx\n", (unsigned long long)box.layout.payload);
  printf("  tag: 0x%x\n", box.layout.tag);
  printf("  quiet: %d\n", box.layout.quiet);
  printf("  exponent: 0x%x\n", box.layout.exponent);
  printf("  sign: %d\n", box.layout.sign);
  printf("  is_nanbox: %d\n", CHIBA_assert_is_nanbox(box.f64));

  cstr ss = CHIBA_nanbox_to_ptr(box.f64);
  printf("Unboxed String: %s, Address: %p\n", ss, (anyptr)ss);
  return 0;
}