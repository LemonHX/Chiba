#pragma once
#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"

// 只插入的动态数组 - 用于存储函数指针
// 特点：不支持删除，只追加，内存连续，缓存友好
// 专门用于 __attribute__((constructor)) 场景

// 函数指针向量结构
typedef struct {
  void **data;  // 函数指针数组
  u64 capacity; // 容量
  u64 count;    // 已存储的函数指针数量
} VVec;

// 创建向量（初始容量较大，适合 constructor 场景）
UTILS VVec *vvec_create() {
  u64 initial_capacity = 256; // 初始容量256，足够大

  VVec *vec = (VVec *)malloc(sizeof(VVec));
  if (vec == NULL) {
    return NULL;
  }

  vec->data = (void **)malloc(sizeof(void *) * initial_capacity);
  if (vec->data == NULL) {
    free(vec);
    return NULL;
  }

  vec->capacity = initial_capacity;
  vec->count = 0;

  return vec;
}

// 扩容向量（容量翻倍）
UTILS void vvec_resize(VVec *vec) {
  if (vec == NULL) {
    fprintf(stderr,
            "%s%s%s:%d FATAL ERROR: vvec_resize called with NULL vector%s\n",
            ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
    abort();
  }

  u64 new_capacity = vec->capacity << 1; // 容量翻倍
  void **new_data = (void **)realloc(vec->data, sizeof(void *) * new_capacity);

  if (new_data == NULL) {
    fprintf(stderr,
            "%s%s%s:%d FATAL ERROR: vvec_resize failed to allocate memory%s\n",
            ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
    abort();
  }

  vec->data = new_data;
  vec->capacity = new_capacity;
}

// 追加函数指针
UTILS u64 vvec_push(VVec *vec, void *func_ptr) {
  if (vec == NULL) {
    fprintf(stderr,
            "%s%s%s:%d FATAL ERROR: vvec_push called with NULL vector%s\n",
            ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
    abort();
  }

  // 检查是否需要扩容
  if (vec->count >= vec->capacity) {
    vvec_resize(vec);
  }

  vec->data[vec->count] = func_ptr;
  u64 ret = vec->count;
  vec->count++;

  return ret;
}

// 获取指定索引的函数指针
UTILS void *vvec_get(VVec *vec, u64 index) {
  if (vec == NULL || index >= vec->count) {
    return NULL;
  }
  return vec->data[index];
}

// 获取向量大小
UTILS u64 vvec_size(VVec *vec) { return vec ? vec->count : 0; }

// 获取向量容量
UTILS u64 vvec_capacity(VVec *vec) { return vec ? vec->capacity : 0; }

// 检查向量是否为空
UTILS int vvec_is_empty(VVec *vec) { return vec == NULL || vec->count == 0; }

// 遍历向量中的所有函数指针
#define VVEC_FOREACH(vec, func_ptr_var)                                        \
  for (u64 _i = 0; _i < (vec)->count && ((func_ptr_var) = (vec)->data[_i], 1); \
       _i++)

// 类型安全的遍历宏（带类型转换）
#define VVEC_FOREACH_TYPED(vec, func_type, func_ptr_var)                       \
  for (u64 _i = 0;                                                             \
       _i < (vec)->count && ((func_ptr_var) = (func_type)(vec)->data[_i], 1);  \
       _i++)

// 便捷宏：创建并初始化向量
#define VVEC_CREATE() vvec_create()

// 便捷宏：追加函数指针（带类型转换）
#define VVEC_PUSH(vec, func_ptr) vvec_push(vec, (void *)(func_ptr))
