#pragma once
#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"

PRIVATE u64 GLOBAL_POSTSTART_REGISTER_CALLBACKS_SIZE = 0;
PRIVATE void* GLOBAL_POSTSTART_REGISTER_CALLBACKS[PRESTART_POST_REGISTER_CALLBACK_SIZE];

UTILS void register_poststart_callback(void (*callback)(void)) {
  if (GLOBAL_POSTSTART_REGISTER_CALLBACKS_SIZE >=
      PRESTART_POST_REGISTER_CALLBACK_SIZE) {
    fprintf(stderr, "Error: Exceeded maximum post-start register callbacks\n");
    abort();
  }
  GLOBAL_POSTSTART_REGISTER_CALLBACKS
      [GLOBAL_POSTSTART_REGISTER_CALLBACKS_SIZE++] = (void *)callback;
}

UTILS void INIT() {
    for (u64 i = 0; i < GLOBAL_POSTSTART_REGISTER_CALLBACKS_SIZE; i++) {
        void (*callback)(void) =
            (void (*)(void))GLOBAL_POSTSTART_REGISTER_CALLBACKS[i];
        callback();
    }
}
