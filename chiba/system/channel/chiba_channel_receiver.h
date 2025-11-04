#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Channel - Receiver 实现
//////////////////////////////////////////////////////////////////////////////////

#include "chiba_channel_core.h"
#include "chiba_channel_intf.h"

//////////////////////////////////////////////////////////////////////////////////
// Receiver 克隆和释放
//////////////////////////////////////////////////////////////////////////////////

PUBLIC chiba_receiver_t *chiba_receiver_clone(chiba_receiver_t *rx) {
  if (!rx || !rx->chan)
    return NULL;

  // 引用计数递增
  atomic_fetch_add_explicit(&rx->chan->receiver_count, 1, memory_order_relaxed);

  // 创建新的 receiver 包装
  chiba_receiver_t *new_rx =
      (chiba_receiver_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_receiver_t));
  if (!new_rx) {
    atomic_fetch_sub_explicit(&rx->chan->receiver_count, 1,
                              memory_order_relaxed);
    return NULL;
  }

  new_rx->chan = rx->chan;
  return new_rx;
}

PUBLIC void chiba_receiver_drop(chiba_receiver_t **rx) {
  printf("chiba_receiver_drop called\n");
  if (!rx || !*rx || !(*rx)->chan)
    return;

  chiba_channel_t *chan = (*rx)->chan;

  // 引用计数递减
  u64 old_count =
      atomic_fetch_sub_explicit(&chan->receiver_count, 1, memory_order_release);

  // 如果是最后一个 receiver,断开连接
  if (old_count == 1) {
    chiba_channel_disconnect(chan);

    // 检查是否需要销毁 channel
    u64 tx_count =
        atomic_load_explicit(&chan->sender_count, memory_order_acquire);
    if (tx_count == 0) {
      chiba_channel_destroy(chan);
    }
  }

  // 释放 receiver 包装
  CHIBA_INTERNAL_free(*rx);
}

//////////////////////////////////////////////////////////////////////////////////
// Receiver 接收操作
//////////////////////////////////////////////////////////////////////////////////

PUBLIC i32 chiba_receiver_try_recv(chiba_receiver_t *rx, void **data_out) {
  if (!rx || !rx->chan || !data_out)
    return CHIBA_CHAN_DISCONNECTED;

  chiba_channel_t *chan = rx->chan;

  // 尝试出队
  if (chiba_channel_dequeue(chan, data_out)) {
    return CHIBA_CHAN_OK;
  }

  // 队列为空,检查是否断开
  if (chiba_channel_is_disconnected(chan)) {
    return CHIBA_CHAN_DISCONNECTED;
  }

  // 队列空但未断开
  return CHIBA_CHAN_EMPTY;
}

//////////////////////////////////////////////////////////////////////////////////
// Receiver 查询操作
//////////////////////////////////////////////////////////////////////////////////

PUBLIC bool chiba_receiver_is_disconnected(chiba_receiver_t *rx) {
  if (!rx || !rx->chan)
    return true;
  return chiba_channel_is_disconnected(rx->chan);
}

PUBLIC bool chiba_receiver_is_empty(chiba_receiver_t *rx) {
  if (!rx || !rx->chan)
    return true;
  return chiba_channel_is_empty(rx->chan);
}

PUBLIC bool chiba_receiver_is_full(chiba_receiver_t *rx) {
  if (!rx || !rx->chan)
    return false;
  return chiba_channel_is_full(rx->chan);
}

PUBLIC u64 chiba_receiver_len(chiba_receiver_t *rx) {
  if (!rx || !rx->chan)
    return 0;
  return chiba_channel_len(rx->chan);
}

PUBLIC u64 chiba_receiver_capacity(chiba_receiver_t *rx) {
  if (!rx || !rx->chan)
    return 0;
  return chiba_channel_capacity(rx->chan);
}
