#pragma once
#include "error.h"
#include "../utils/c_missing_utils.h"

// return_var is the pointer to the return value space
// ctx is for closure context or stack-less coroutine context
// self is the pointer to the object instance for method calls
// args is the pointer to the arguments struct
typedef C8NS(APIResult) (*C8NS(API_FUNC_PTR))(
    void *return_var, 
    void *ctx, 
    void *self,
    void *args
);