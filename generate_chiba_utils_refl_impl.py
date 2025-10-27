#!/usr/bin/env python3
"""
生成 DECLARE_CHIBA_STRUCT_1 到 DECLARE_CHIBA_STRUCT_32 的宏定义
"""

def generate_field_list(count, prefix="field"):
    """生成字段列表，如: field1, field2, field3"""
    return ", ".join(f"{prefix}{i}" for i in range(1, count + 1))

def generate_expand_fields(count):
    """生成 EXPAND_FIELD 展开，如: EXPAND_FIELD field1 EXPAND_FIELD field2"""
    return " ".join(f"EXPAND_FIELD field{i}" for i in range(1, count + 1))

def generate_expand_metadata(count):
    """生成 EXPAND_FIELD_METADATA 展开"""
    parts = [f"EXPAND_FIELD_METADATA_WITH_OFFSET(struct CHIBA_##struct_name##_struct, field{i})" for i in range(1, count + 1)]
    # 每3个字段换行，保持代码美观
    result = []
    for i in range(0, len(parts), 3):
        chunk = parts[i:i+3]
        if i + 3 < len(parts):
            result.append(" ".join(chunk) + " \\")
        else:
            result.append(" ".join(chunk))
    return "\n      ".join(result)

def generate_macro_select(max_count):
    """生成 MACRO_SELECT 宏的参数列表"""
    # 生成 _1, _2, _3, ...
    params = ", ".join(f"_{i}" for i in range(1, max_count + 1))
    # 生成 DECLARE_CHIBA_STRUCT_32, ..., DECLARE_CHIBA_STRUCT_1
    names = ", ".join(f"DECLARE_CHIBA_STRUCT_{i}" for i in range(max_count, 0, -1))
    return f"#define MACRO_SELECT({params}, NAME, ...) NAME"

def generate_struct_macro(n):
    """生成单个 DECLARE_CHIBA_STRUCT_N 宏"""
    fields_param = generate_field_list(n)
    expand_fields = generate_expand_fields(n)
    expand_metadata = generate_expand_metadata(n)
    
    macro = f"""#define DECLARE_CHIBA_STRUCT_{n}(struct_name, {fields_param})"""
    macro += f"""  \\
  DECLARE_CHIBA_STRUCT_FORWARD(struct_name);                                   \\
  typedef struct __attribute__((aligned(8))) CHIBA_##struct_name##_struct {{                                \\
    const C8NS(ReflMetaInfo) *metainfo;                                            \\
    {expand_fields}                \\
  }} CHIBA_##struct_name;                                                       \\
  const C8NS(ReflFieldMetaInfo) CHIBA_##struct_name##_FIELD_METAINFO[] = {{        \\
      {expand_metadata}}};              \\
  DECLARE_CHIBA_METADATA(struct_name);                                         \\
  PRIVATE VHashTable *CHIBA_##struct_name##_dyn_vtable; \\
  BEFORE_START void init_CHIBA_##struct_name##_refl(void) {{ \\
  CHIBA_##struct_name##_dyn_vtable = vhashtable_create(); \\
  }} \\
  DECLARE_CHIBA_CONSTRUCTORS(struct_name)
"""
    return macro

def generate_declare_chiba_struct_macro(max_count):
    """生成主 DECLARE_CHIBA_STRUCT 宏"""
    # 生成所有的 DECLARE_CHIBA_STRUCT_N 引用
    macro_names = ", ".join(f"DECLARE_CHIBA_STRUCT_{i}" for i in range(max_count, 0, -1))
    
    macro = f"""// 主宏 - 根据参数数量选择正确的版本
#define DECLARE_CHIBA_STRUCT(struct_name, ...)                                 \\
  MACRO_SELECT(__VA_ARGS__, {macro_names})(struct_name, __VA_ARGS__)
"""
    return macro

def generate_full_file(max_count):
    """生成完整的头文件内容"""
    output = """#pragma once
#include "chiba_utils_basic_types.h"
#include "chiba_utils_math.h"
#include "chiba_utils_visibility.h"

"""
    
    # 生成所有的 DECLARE_CHIBA_STRUCT_N 宏
    for i in range(1, max_count + 1):
        output += generate_struct_macro(i)
        output += "\n\n"
    
    # 添加辅助宏
    output += """// 辅助宏
#define DECLARE_CHIBA_STRUCT_FORWARD(struct_name)                              \\
  struct CHIBA_##struct_name##_struct;

#define EXPAND_FIELD(field_name, field_type) field_type field_name;
#define EXPAND_FIELD_METADATA_WITH_OFFSET(struct_name, field)                                 \\
{.offset = offsetof(struct_name, EXPAND_FIELD_EXTRACT_FIELD_NAME field), EXPAND_FIELD_METADATA field
#define EXPAND_FIELD_EXTRACT_FIELD_NAME(field_name, field_type) field_name
#define EXPAND_FIELD_METADATA(field_name, field_type)                          \\
 .name = #field_name, .type = #field_type, .size = sizeof(field_type)},

#define DECLARE_CHIBA_METADATA(struct_name)                                    \\
  const C8NS(ReflMetaInfo) CHIBA_##struct_name##_METAINFO = {                      \\
      .fields = CHIBA_##struct_name##_FIELD_METAINFO,                          \\
      .field_count = countof(CHIBA_##struct_name##_FIELD_METAINFO),            \\
  };

#define DECLARE_CHIBA_CONSTRUCTORS(struct_name)                                \\
  UTILS CHIBA_##struct_name _MK_CHIBA_##struct_name(CHIBA_##struct_name val) {  \\
    val.metainfo = &CHIBA_##struct_name##_METAINFO;                            \\
    return val;                                                                \\
  }                                                                            \\
  UTILS CHIBA_##struct_name *_NEW_CHIBA_##struct_name(CHIBA_##struct_name val,  \\
                                                     void *(*alloc_f)(u64)) {  \\
    CHIBA_##struct_name new_val = _MK_CHIBA_##struct_name(val);                 \\
    CHIBA_##struct_name *p =                                                   \\
        (CHIBA_##struct_name *)alloc_f(sizeof(CHIBA_##struct_name));           \\
    *p = new_val;                                                              \\
    return p;                                                                  \\
  }

"""
    
    # 添加主宏
    output += generate_declare_chiba_struct_macro(max_count)
    output += "\n"
    
    # 添加 MACRO_SELECT
    output += generate_macro_select(max_count)
    output += "\n"
    
    return output

if __name__ == "__main__":
    max_count = int(input("请输入要生成的最大字段数量（如 8）："))
    if max_count < 1 or max_count > 256:
        print("❌ 错误：最大字段数量应在 1 到 256 之间")
        exit(1)
    content = generate_full_file(max_count)
    
    # 写入文件
    output_file = "include/utils/chiba_utils_refl_impl.h"
    with open(output_file, "w", encoding="utf-8") as f:
        f.write(content)
    
    print(f"✅ 已生成文件: {output_file}")
