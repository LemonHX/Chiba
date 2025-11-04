#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Channel - 共享数据结构 (内部实现)
//////////////////////////////////////////////////////////////////////////////////

#include "../basic_memory.h"

//////////////////////////////////////////////////////////////////////////////////
// 简单的无锁队列节点 (单向链表)
//////////////////////////////////////////////////////////////////////////////////

typedef struct chiba_chan_node {
  void *data;
  struct chiba_chan_node *next;
} chiba_chan_node_t;

//////////////////////////////////////////////////////////////////////////////////
// 共享 Channel 结构
//////////////////////////////////////////////////////////////////////////////////

typedef struct chiba_channel {
  // 队列头尾指针 (用 mutex 保护)
  chiba_chan_node_t *head;
  chiba_chan_node_t *tail;

  // Bounded channel 的容量 (0 = unbounded)
  u64 capacity;

  // 当前队列长度 (原子操作)
  _Atomic u64 len;

  // 引用计数
  _Atomic u64 sender_count;
  _Atomic u64 receiver_count;

  // 断开标志
  _Atomic bool disconnected;

  // 互斥锁 (保护队列操作)
  pthread_mutex_t mutex;
} chiba_channel_t;

//////////////////////////////////////////////////////////////////////////////////
// 内部辅助函数声明
//////////////////////////////////////////////////////////////////////////////////

// 创建队列节点
PRIVATE chiba_chan_node_t *chiba_chan_node_new(void *data);

// 释放队列节点
PRIVATE void chiba_chan_node_free(chiba_chan_node_t *node);

// 初始化 channel
PRIVATE chiba_channel_t *chiba_channel_new(u64 capacity);

// 销毁 channel (当所有 sender/receiver 都释放后)
PRIVATE void chiba_channel_destroy(chiba_channel_t *chan);

// 入队 (返回是否成功)
PRIVATE bool chiba_channel_enqueue(chiba_channel_t *chan, void *data);

// 出队 (返回 true 且填充 data_out,或返回 false)
PRIVATE bool chiba_channel_dequeue(chiba_channel_t *chan, void **data_out);
