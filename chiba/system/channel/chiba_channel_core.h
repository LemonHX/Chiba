#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Channel - 核心实现
//////////////////////////////////////////////////////////////////////////////////

#include "chiba_channel_shared.h"

//////////////////////////////////////////////////////////////////////////////////
// 队列节点管理
//////////////////////////////////////////////////////////////////////////////////

PRIVATE chiba_chan_node_t *chiba_chan_node_new(void *data) {
  chiba_chan_node_t *node =
      (chiba_chan_node_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_chan_node_t));
  if (!node)
    return NULL;
  node->data = data;
  node->next = NULL;
  return node;
}

PRIVATE void chiba_chan_node_free(chiba_chan_node_t *node) {
  CHIBA_INTERNAL_free(node);
}

//////////////////////////////////////////////////////////////////////////////////
// Channel 创建和销毁
//////////////////////////////////////////////////////////////////////////////////

PRIVATE chiba_channel_t *chiba_channel_new(u64 capacity) {
  chiba_channel_t *chan =
      (chiba_channel_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_channel_t));
  if (!chan)
    return NULL;

  // 初始化队列为空
  chan->head = NULL;
  chan->tail = NULL;

  // 设置容量
  chan->capacity = capacity;

  // 初始化原子变量
  atomic_init(&chan->len, 0);
  atomic_init(&chan->sender_count, 1);
  atomic_init(&chan->receiver_count, 1);
  atomic_init(&chan->disconnected, false);

  // 初始化互斥锁
  pthread_mutex_init(&chan->mutex, NULL);

  return chan;
}

PRIVATE void chiba_channel_destroy(chiba_channel_t *chan) {
  if (!chan)
    return;

  // 释放所有剩余节点
  pthread_mutex_lock(&chan->mutex);
  chiba_chan_node_t *node = chan->head;
  while (node) {
    chiba_chan_node_t *next = node->next;
    chiba_chan_node_free(node);
    node = next;
  }
  pthread_mutex_unlock(&chan->mutex);

  // 销毁互斥锁
  pthread_mutex_destroy(&chan->mutex);

  // 释放 channel
  CHIBA_INTERNAL_free(chan);
}

//////////////////////////////////////////////////////////////////////////////////
// Channel 队列操作
//////////////////////////////////////////////////////////////////////////////////

PRIVATE bool chiba_channel_enqueue(chiba_channel_t *chan, void *data) {
  // 检查是否断开
  if (atomic_load_explicit(&chan->disconnected, memory_order_relaxed)) {
    return false;
  }

  // 检查容量 (bounded channel)
  if (chan->capacity > 0) {
    u64 current_len = atomic_load_explicit(&chan->len, memory_order_relaxed);
    if (current_len >= chan->capacity) {
      return false; // 队列已满
    }
  }

  // 创建新节点
  chiba_chan_node_t *node = chiba_chan_node_new(data);
  if (!node)
    return false;

  // 加锁并入队
  pthread_mutex_lock(&chan->mutex);

  // 再次检查断开状态 (double-check)
  if (atomic_load_explicit(&chan->disconnected, memory_order_relaxed)) {
    pthread_mutex_unlock(&chan->mutex);
    chiba_chan_node_free(node);
    return false;
  }

  // 再次检查容量
  if (chan->capacity > 0 &&
      atomic_load_explicit(&chan->len, memory_order_relaxed) >=
          chan->capacity) {
    pthread_mutex_unlock(&chan->mutex);
    chiba_chan_node_free(node);
    return false;
  }

  // 入队操作
  if (chan->tail) {
    chan->tail->next = node;
    chan->tail = node;
  } else {
    chan->head = node;
    chan->tail = node;
  }

  // 增加长度
  atomic_fetch_add_explicit(&chan->len, 1, memory_order_relaxed);

  pthread_mutex_unlock(&chan->mutex);
  return true;
}

PRIVATE bool chiba_channel_dequeue(chiba_channel_t *chan, void **data_out) {
  pthread_mutex_lock(&chan->mutex);

  // 检查队列是否为空
  if (!chan->head) {
    pthread_mutex_unlock(&chan->mutex);
    return false;
  }

  // 出队操作
  chiba_chan_node_t *node = chan->head;
  chan->head = node->next;
  if (!chan->head) {
    chan->tail = NULL; // 队列空了
  }

  // 减少长度
  atomic_fetch_sub_explicit(&chan->len, 1, memory_order_relaxed);

  pthread_mutex_unlock(&chan->mutex);

  // 提取数据并释放节点
  *data_out = node->data;
  chiba_chan_node_free(node);

  return true;
}

//////////////////////////////////////////////////////////////////////////////////
// Channel 查询操作
//////////////////////////////////////////////////////////////////////////////////

PRIVATE bool chiba_channel_is_disconnected(chiba_channel_t *chan) {
  return atomic_load_explicit(&chan->disconnected, memory_order_seq_cst);
}

PRIVATE bool chiba_channel_is_empty(chiba_channel_t *chan) {
  return atomic_load_explicit(&chan->len, memory_order_relaxed) == 0;
}

PRIVATE bool chiba_channel_is_full(chiba_channel_t *chan) {
  if (chan->capacity == 0)
    return false; // unbounded
  return atomic_load_explicit(&chan->len, memory_order_relaxed) >=
         chan->capacity;
}

PRIVATE u64 chiba_channel_len(chiba_channel_t *chan) {
  return atomic_load_explicit(&chan->len, memory_order_relaxed);
}

PRIVATE u64 chiba_channel_capacity(chiba_channel_t *chan) {
  return chan->capacity == 0 ? UINT64_MAX : chan->capacity;
}

PRIVATE u64 chiba_channel_sender_count(chiba_channel_t *chan) {
  return atomic_load_explicit(&chan->sender_count, memory_order_relaxed);
}

PRIVATE u64 chiba_channel_receiver_count(chiba_channel_t *chan) {
  return atomic_load_explicit(&chan->receiver_count, memory_order_relaxed);
}

//////////////////////////////////////////////////////////////////////////////////
// 断开操作
//////////////////////////////////////////////////////////////////////////////////

PRIVATE void chiba_channel_disconnect(chiba_channel_t *chan) {
  atomic_store_explicit(&chan->disconnected, true, memory_order_release);
}
