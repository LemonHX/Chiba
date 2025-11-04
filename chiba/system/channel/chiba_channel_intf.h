#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Channel - C port of Rust flume MPMC channel
//////////////////////////////////////////////////////////////////////////////////

#include "../../basic_types.h"

// try_send 返回值
#define CHIBA_CHAN_OK 0           // 成功
#define CHIBA_CHAN_FULL 1         // Channel 满了 (仅 bounded)
#define CHIBA_CHAN_DISCONNECTED 2 // 对端已关闭

// try_recv 返回值
#define CHIBA_CHAN_EMPTY 3 // Channel 空

// 阻塞操作额外的超时错误
#define CHIBA_CHAN_TIMEOUT 4 // 超时

//////////////////////////////////////////////////////////////////////////////////
// 不透明类型 (对用户隐藏内部实现)
//////////////////////////////////////////////////////////////////////////////////
typedef struct chiba_channel chiba_channel_t;

typedef struct chiba_sender {
  chiba_channel_t *chan;
} chiba_sender_t;

typedef struct chiba_receiver {
  chiba_channel_t *chan;
} chiba_receiver_t;

//////////////////////////////////////////////////////////////////////////////////
// Channel 创建和销毁
//////////////////////////////////////////////////////////////////////////////////

/**
 * 创建无界 channel (unbounded)
 * @param tx_out 输出参数: sender 指针
 * @param rx_out 输出参数: receiver 指针
 */
PUBLIC void chiba_channel_unbounded(chiba_sender_t **tx_out,
                                    chiba_receiver_t **rx_out);

/**
 * 创建有界 channel (bounded)
 * @param capacity channel 最大容量 (0 表示 rendezvous)
 * @param tx_out 输出参数: sender 指针
 * @param rx_out 输出参数: receiver 指针
 */
PUBLIC void chiba_channel_bounded(u64 capacity, chiba_sender_t **tx_out,
                                  chiba_receiver_t **rx_out);

//////////////////////////////////////////////////////////////////////////////////
// Sender 操作
//////////////////////////////////////////////////////////////////////////////////

/**
 * 克隆 sender (引用计数递增)
 * @return 新的 sender 指针
 */
PUBLIC chiba_sender_t *chiba_sender_clone(chiba_sender_t *tx);

/**
 * 释放 sender (引用计数递减,为 0 时通知 receiver 断开)
 */
PUBLIC void chiba_sender_drop(chiba_sender_t **tx);

/**
 * 非阻塞发送 (立即返回)
 * @param tx sender
 * @param data 要发送的指针 (void*)
 * @return CHIBA_CHAN_OK | CHIBA_CHAN_FULL | CHIBA_CHAN_DISCONNECTED
 */
PUBLIC i32 chiba_sender_try_send(chiba_sender_t *tx, void *data);

/**
 * 检查 receiver 是否已全部断开
 */
PUBLIC bool chiba_sender_is_disconnected(chiba_sender_t *tx);

/**
 * 检查 channel 是否为空
 */
PUBLIC bool chiba_sender_is_empty(chiba_sender_t *tx);

/**
 * 检查 channel 是否已满 (unbounded 永远返回 false)
 */
PUBLIC bool chiba_sender_is_full(chiba_sender_t *tx);

/**
 * 获取当前 channel 中的消息数
 */
PUBLIC u64 chiba_sender_len(chiba_sender_t *tx);

/**
 * 获取 channel 容量 (unbounded 返回 UINT64_MAX)
 */
PUBLIC u64 chiba_sender_capacity(chiba_sender_t *tx);

//////////////////////////////////////////////////////////////////////////////////
// Receiver 操作
//////////////////////////////////////////////////////////////////////////////////

/**
 * 克隆 receiver (引用计数递增)
 * @return 新的 receiver 指针
 */
PUBLIC chiba_receiver_t *chiba_receiver_clone(chiba_receiver_t *rx);

/**
 * 释放 receiver (引用计数递减,为 0 时通知 sender 断开)
 */
PUBLIC void chiba_receiver_drop(chiba_receiver_t **rx);

/**
 * 非阻塞接收 (立即返回)
 * @param rx receiver
 * @param data_out 输出参数: 接收到的指针
 * @return CHIBA_CHAN_OK | CHIBA_CHAN_EMPTY | CHIBA_CHAN_DISCONNECTED
 */
PUBLIC i32 chiba_receiver_try_recv(chiba_receiver_t *rx, void **data_out);

/**
 * 检查 sender 是否已全部断开
 */
PUBLIC bool chiba_receiver_is_disconnected(chiba_receiver_t *rx);

/**
 * 检查 channel 是否为空
 */
PUBLIC bool chiba_receiver_is_empty(chiba_receiver_t *rx);

/**
 * 检查 channel 是否已满
 */
PUBLIC bool chiba_receiver_is_full(chiba_receiver_t *rx);

/**
 * 获取当前 channel 中的消息数
 */
PUBLIC u64 chiba_receiver_len(chiba_receiver_t *rx);

/**
 * 获取 channel 容量
 */
PUBLIC u64 chiba_receiver_capacity(chiba_receiver_t *rx);

//////////////////////////////////////////////////////////////////////////////////
// 辅助宏
//////////////////////////////////////////////////////////////////////////////////

// 用于判断操作是否成功
#define CHIBA_CHAN_IS_OK(ret) ((ret) == CHIBA_CHAN_OK)

// 用于判断是否断开连接
#define CHIBA_CHAN_IS_DISCONNECTED(ret) ((ret) == CHIBA_CHAN_DISCONNECTED)
