#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Channel - 总实现 (集成所有模块)
//
// 模块组织:
// - chiba_channel_shared.h: 共享数据结构定义
// - chiba_channel_core.h: 核心队列操作实现
// - chiba_channel_sender.h: Sender 操作
// - chiba_channel_receiver.h: Receiver 操作
// - chiba_channel_create.h: Channel 创建
//////////////////////////////////////////////////////////////////////////////////

#include "chiba_channel.h"

// 包含所有实现模块
#include "chiba_channel_core.h"
#include "chiba_channel_create.h"
#include "chiba_channel_receiver.h"
#include "chiba_channel_sender.h"
#include "chiba_channel_shared.h"
