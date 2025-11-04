#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Channel - Channel 创建
//////////////////////////////////////////////////////////////////////////////////

#include "chiba_channel_core.h"
#include "chiba_channel_intf.h"

//////////////////////////////////////////////////////////////////////////////////
// 创建 unbounded channel
//////////////////////////////////////////////////////////////////////////////////

PUBLIC void chiba_channel_unbounded(chiba_sender_t **tx_out,
                                    chiba_receiver_t **rx_out) {
  if (!tx_out || !rx_out)
    return;

  // 创建 channel (capacity = 0 表示 unbounded)
  chiba_channel_t *chan = chiba_channel_new(0);
  if (!chan) {
    *tx_out = NULL;
    *rx_out = NULL;
    return;
  }

  // 创建 sender
  chiba_sender_t *tx =
      (chiba_sender_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_sender_t));
  if (!tx) {
    chiba_channel_destroy(chan);
    *tx_out = NULL;
    *rx_out = NULL;
    return;
  }
  tx->chan = chan;

  // 创建 receiver
  chiba_receiver_t *rx =
      (chiba_receiver_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_receiver_t));
  if (!rx) {
    CHIBA_INTERNAL_free(tx);
    chiba_channel_destroy(chan);
    *tx_out = NULL;
    *rx_out = NULL;
    return;
  }
  rx->chan = chan;

  *tx_out = tx;
  *rx_out = rx;
}

//////////////////////////////////////////////////////////////////////////////////
// 创建 bounded channel
//////////////////////////////////////////////////////////////////////////////////

PUBLIC void chiba_channel_bounded(u64 capacity, chiba_sender_t **tx_out,
                                  chiba_receiver_t **rx_out) {
  if (!tx_out || !rx_out)
    return;

  // 创建 channel
  chiba_channel_t *chan = chiba_channel_new(capacity);
  if (!chan) {
    *tx_out = NULL;
    *rx_out = NULL;
    return;
  }

  // 创建 sender
  chiba_sender_t *tx =
      (chiba_sender_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_sender_t));
  if (!tx) {
    chiba_channel_destroy(chan);
    *tx_out = NULL;
    *rx_out = NULL;
    return;
  }
  tx->chan = chan;

  // 创建 receiver
  chiba_receiver_t *rx =
      (chiba_receiver_t *)CHIBA_INTERNAL_malloc(sizeof(chiba_receiver_t));
  if (!rx) {
    CHIBA_INTERNAL_free(tx);
    chiba_channel_destroy(chan);
    *tx_out = NULL;
    *rx_out = NULL;
    return;
  }
  rx->chan = chan;

  *tx_out = tx;
  *rx_out = rx;
}
