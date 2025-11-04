#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Channel - Sender 实现
//////////////////////////////////////////////////////////////////////////////////

#include "chiba_channel.h"
#include "chiba_channel_core.h"

//////////////////////////////////////////////////////////////////////////////////
// Sender 克隆和释放
//////////////////////////////////////////////////////////////////////////////////

PUBLIC chiba_sender_t *chiba_sender_clone(chiba_sender_t *tx) {
  if (!tx || !tx->chan)
    return NULL;

  // 引用计数递增
  atomic_fetch_add_explicit(&tx->chan->sender_count, 1, memory_order_relaxed);

  // 创建新的 sender 包装
  chiba_sender_t *new_tx =
      (chiba_sender_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_sender_t));
  if (!new_tx) {
    atomic_fetch_sub_explicit(&tx->chan->sender_count, 1, memory_order_relaxed);
    return NULL;
  }

  new_tx->chan = tx->chan;
  return new_tx;
}

PUBLIC void chiba_sender_drop(chiba_sender_t **tx) {
  printf("chiba_sender_drop called\n");
  if (!tx || !*tx || !(*tx)->chan)
    return;

  chiba_channel_t *chan = (*tx)->chan;

  // 引用计数递减
  u64 old_count =
      atomic_fetch_sub_explicit(&chan->sender_count, 1, memory_order_release);

  // 如果是最后一个 sender,断开连接
  if (old_count == 1) {
    chiba_channel_disconnect(chan);

    // 检查是否需要销毁 channel
    u64 rx_count =
        atomic_load_explicit(&chan->receiver_count, memory_order_acquire);
    if (rx_count == 0) {
      chiba_channel_destroy(chan);
    }
  }

  // 释放 sender 包装
  CHIBA_INTERNAL_free(*tx);
}

//////////////////////////////////////////////////////////////////////////////////
// Sender 发送操作
//////////////////////////////////////////////////////////////////////////////////

PUBLIC i32 chiba_sender_try_send(chiba_sender_t *tx, void *data) {
  if (!tx || !tx->chan)
    return CHIBA_CHAN_DISCONNECTED;

  chiba_channel_t *chan = tx->chan;

  // 检查是否断开
  if (chiba_channel_is_disconnected(chan)) {
    return CHIBA_CHAN_DISCONNECTED;
  }

  // 尝试入队
  if (chiba_channel_enqueue(chan, data)) {
    return CHIBA_CHAN_OK;
  }

  // 再次检查是否断开 (可能在入队失败后断开)
  if (chiba_channel_is_disconnected(chan)) {
    return CHIBA_CHAN_DISCONNECTED;
  }

  // 队列满了
  return CHIBA_CHAN_FULL;
}

//////////////////////////////////////////////////////////////////////////////////
// Sender 查询操作
//////////////////////////////////////////////////////////////////////////////////

PUBLIC bool chiba_sender_is_disconnected(chiba_sender_t *tx) {
  if (!tx || !tx->chan)
    return true;
  return chiba_channel_is_disconnected(tx->chan);
}

PUBLIC bool chiba_sender_is_empty(chiba_sender_t *tx) {
  if (!tx || !tx->chan)
    return true;
  return chiba_channel_is_empty(tx->chan);
}

PUBLIC bool chiba_sender_is_full(chiba_sender_t *tx) {
  if (!tx || !tx->chan)
    return false;
  return chiba_channel_is_full(tx->chan);
}

PUBLIC u64 chiba_sender_len(chiba_sender_t *tx) {
  if (!tx || !tx->chan)
    return 0;
  return chiba_channel_len(tx->chan);
}

PUBLIC u64 chiba_sender_capacity(chiba_sender_t *tx) {
  if (!tx || !tx->chan)
    return 0;
  return chiba_channel_capacity(tx->chan);
}
