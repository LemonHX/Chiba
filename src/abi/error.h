#pragma once

#include "basic_type.h"

typedef C8STRUCT C8NS(APIError) {
  cstr msg;
  cstr file;
  u64 line;
} C8NS(APIError);

typedef C8STRUCT C8NS(APIResult) {
  C8NS(APIError) err;
  bool ok;
} C8NS(APIResult);