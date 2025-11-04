// Test file for chiba channel
#include "chiba/system/channel/chiba_channel_impl.h"
#include "chiba/system/channel/chiba_channel_intf.h"
#include <assert.h>
#include <stdio.h>

void test_unbounded_channel() {
  printf("Testing unbounded channel...\n");

  chiba_sender_t *tx DEFER(chiba_sender_drop);
  chiba_receiver_t *rx DEFER(chiba_receiver_drop);

  // 创建 unbounded channel
  chiba_channel_unbounded(&tx, &rx);
  assert(tx != NULL);
  assert(rx != NULL);

  // 发送数据
  int data1 = 42;
  int data2 = 100;
  assert(chiba_sender_try_send(tx, &data1) == CHIBA_CHAN_OK);
  assert(chiba_sender_try_send(tx, &data2) == CHIBA_CHAN_OK);

  // 检查长度
  assert(chiba_sender_len(tx) == 2);
  assert(chiba_receiver_len(rx) == 2);

  // 接收数据
  void *received;
  assert(chiba_receiver_try_recv(rx, &received) == CHIBA_CHAN_OK);
  assert(*(int *)received == 42);

  assert(chiba_receiver_try_recv(rx, &received) == CHIBA_CHAN_OK);
  assert(*(int *)received == 100);

  // 空队列
  assert(chiba_receiver_try_recv(rx, &received) == CHIBA_CHAN_EMPTY);

  // 清理
  printf("✓ Unbounded channel test passed!\n");
}

void test_bounded_channel() {
  printf("Testing bounded channel...\n");

  chiba_sender_t *tx DEFER(chiba_sender_drop);
  chiba_receiver_t *rx DEFER(chiba_receiver_drop);

  // 创建 bounded channel (容量为 2)
  chiba_channel_bounded(2, &tx, &rx);
  assert(tx != NULL);
  assert(rx != NULL);

  // 发送数据直到满
  int data1 = 1;
  int data2 = 2;
  int data3 = 3;

  assert(chiba_sender_try_send(tx, &data1) == CHIBA_CHAN_OK);
  assert(chiba_sender_try_send(tx, &data2) == CHIBA_CHAN_OK);

  // 队列满了
  assert(chiba_sender_is_full(tx));
  assert(chiba_sender_try_send(tx, &data3) == CHIBA_CHAN_FULL);

  // 接收一个数据,腾出空间
  void *received;
  assert(chiba_receiver_try_recv(rx, &received) == CHIBA_CHAN_OK);
  assert(*(int *)received == 1);

  // 现在可以再发送一个
  assert(chiba_sender_try_send(tx, &data3) == CHIBA_CHAN_OK);

  printf("✓ Bounded channel test passed!\n");
}

void test_disconnection() {
  printf("Testing disconnection...\n");

  chiba_sender_t *tx;
  chiba_receiver_t *rx;

  chiba_channel_unbounded(&tx, &rx);

  // 先发送一些数据
  int data = 123;
  chiba_sender_try_send(tx, &data);

  // 断开 sender
  chiba_sender_drop(&tx);

  // receiver 仍然可以接收已发送的数据
  void *received;
  assert(chiba_receiver_try_recv(rx, &received) == CHIBA_CHAN_OK);
  assert(*(int *)received == 123);

  // 之后返回 disconnected
  assert(chiba_receiver_try_recv(rx, &received) == CHIBA_CHAN_DISCONNECTED);

  chiba_receiver_drop(&rx);

  printf("✓ Disconnection test passed!\n");
}

void test_clone() {
  printf("Testing clone...\n");

  chiba_sender_t *tx1;
  chiba_receiver_t *rx1;

  chiba_channel_unbounded(&tx1, &rx1);

  // 克隆 sender 和 receiver
  chiba_sender_t *tx2 = chiba_sender_clone(tx1);
  chiba_receiver_t *rx2 = chiba_receiver_clone(rx1);

  assert(tx2 != NULL);
  assert(rx2 != NULL);

  // 从 tx1 发送,从 rx2 接收
  int data = 999;
  assert(chiba_sender_try_send(tx1, &data) == CHIBA_CHAN_OK);

  void *received;
  assert(chiba_receiver_try_recv(rx2, &received) == CHIBA_CHAN_OK);
  assert(*(int *)received == 999);

  // 清理
  chiba_sender_drop(&tx1);
  chiba_sender_drop(&tx2);
  chiba_receiver_drop(&rx1);
  chiba_receiver_drop(&rx2);

  printf("✓ Clone test passed!\n");
}

// ============ 多线程测试 ============

#include <pthread.h>

typedef struct {
  chiba_sender_t *tx;
  int thread_id;
  int count;
} sender_args_t;

typedef struct {
  chiba_receiver_t *rx;
  int thread_id;
  int expected_count;
  _Atomic int *received_count;
} receiver_args_t;

void *sender_thread(void *arg) {
  sender_args_t *args = (sender_args_t *)arg;

  for (int i = 0; i < args->count; i++) {
    int *data = (int *)CHIBA_INTERNAL_malloc(sizeof(int));
    *data = args->thread_id * 10000 + i;

    // 发送,如果满了就重试
    while (chiba_sender_try_send(args->tx, data) == CHIBA_CHAN_FULL) {
      // 模拟 busy-wait,实际协程会 yield
    }
  }

  return NULL;
}

void *receiver_thread(void *arg) {
  receiver_args_t *args = (receiver_args_t *)arg;
  int count = 0;

  while (count < args->expected_count) {
    void *data;
    i32 ret = chiba_receiver_try_recv(args->rx, &data);

    if (ret == CHIBA_CHAN_OK) {
      count++;
      atomic_fetch_add_explicit(args->received_count, 1, memory_order_relaxed);
      CHIBA_INTERNAL_free(data); // 释放发送方分配的内存
    } else if (ret == CHIBA_CHAN_DISCONNECTED) {
      break;
    }
    // EMPTY 就继续重试
  }

  return NULL;
}

void test_mpmc_unbounded() {
  printf("Testing MPMC unbounded channel...\n");

  chiba_sender_t *tx;
  chiba_receiver_t *rx;
  chiba_channel_unbounded(&tx, &rx);

  const int NUM_SENDERS = 4;
  const int NUM_RECEIVERS = 3;
  const int MSGS_PER_SENDER = 1000;
  const int TOTAL_MSGS = NUM_SENDERS * MSGS_PER_SENDER;

  pthread_t sender_threads[NUM_SENDERS];
  pthread_t receiver_threads[NUM_RECEIVERS];
  sender_args_t sender_args[NUM_SENDERS];
  receiver_args_t receiver_args[NUM_RECEIVERS];

  _Atomic int total_received = 0;

  // 创建多个 sender 和 receiver
  chiba_sender_t *senders[NUM_SENDERS];
  chiba_receiver_t *receivers[NUM_RECEIVERS];

  senders[0] = tx;
  receivers[0] = rx;

  for (int i = 1; i < NUM_SENDERS; i++) {
    senders[i] = chiba_sender_clone(tx);
  }

  for (int i = 1; i < NUM_RECEIVERS; i++) {
    receivers[i] = chiba_receiver_clone(rx);
  }

  // 启动 sender 线程
  for (int i = 0; i < NUM_SENDERS; i++) {
    sender_args[i].tx = senders[i];
    sender_args[i].thread_id = i;
    sender_args[i].count = MSGS_PER_SENDER;
    pthread_create(&sender_threads[i], NULL, sender_thread, &sender_args[i]);
  }

  // 启动 receiver 线程
  int msgs_per_receiver = TOTAL_MSGS / NUM_RECEIVERS;
  for (int i = 0; i < NUM_RECEIVERS; i++) {
    receiver_args[i].rx = receivers[i];
    receiver_args[i].thread_id = i;
    receiver_args[i].expected_count =
        msgs_per_receiver +
        (i == NUM_RECEIVERS - 1 ? TOTAL_MSGS % NUM_RECEIVERS : 0);
    receiver_args[i].received_count = &total_received;
    pthread_create(&receiver_threads[i], NULL, receiver_thread,
                   &receiver_args[i]);
  }

  // 等待所有 sender 完成
  for (int i = 0; i < NUM_SENDERS; i++) {
    pthread_join(sender_threads[i], NULL);
    chiba_sender_drop(&senders[i]);
  }

  // 等待所有 receiver 完成
  for (int i = 0; i < NUM_RECEIVERS; i++) {
    pthread_join(receiver_threads[i], NULL);
    chiba_receiver_drop(&receivers[i]);
  }

  // 验证所有消息都被接收
  int final_count = atomic_load_explicit(&total_received, memory_order_relaxed);
  printf("  Total sent: %d, Total received: %d\n", TOTAL_MSGS, final_count);
  assert(final_count == TOTAL_MSGS);

  printf("✓ MPMC unbounded channel test passed!\n");
}

void test_mpmc_bounded() {
  printf("Testing MPMC bounded channel...\n");

  chiba_sender_t *tx;
  chiba_receiver_t *rx;
  chiba_channel_bounded(100, &tx, &rx); // 小容量,会有竞争

  const int NUM_SENDERS = 3;
  const int NUM_RECEIVERS = 2;
  const int MSGS_PER_SENDER = 500;
  const int TOTAL_MSGS = NUM_SENDERS * MSGS_PER_SENDER;

  pthread_t sender_threads[NUM_SENDERS];
  pthread_t receiver_threads[NUM_RECEIVERS];
  sender_args_t sender_args[NUM_SENDERS];
  receiver_args_t receiver_args[NUM_RECEIVERS];

  _Atomic int total_received = 0;

  chiba_sender_t *senders[NUM_SENDERS];
  chiba_receiver_t *receivers[NUM_RECEIVERS];

  senders[0] = tx;
  receivers[0] = rx;

  for (int i = 1; i < NUM_SENDERS; i++) {
    senders[i] = chiba_sender_clone(tx);
  }

  for (int i = 1; i < NUM_RECEIVERS; i++) {
    receivers[i] = chiba_receiver_clone(rx);
  }

  // 启动 receiver 线程 (先启动,避免 sender 阻塞)
  int msgs_per_receiver = TOTAL_MSGS / NUM_RECEIVERS;
  for (int i = 0; i < NUM_RECEIVERS; i++) {
    receiver_args[i].rx = receivers[i];
    receiver_args[i].thread_id = i;
    receiver_args[i].expected_count =
        msgs_per_receiver +
        (i == NUM_RECEIVERS - 1 ? TOTAL_MSGS % NUM_RECEIVERS : 0);
    receiver_args[i].received_count = &total_received;
    pthread_create(&receiver_threads[i], NULL, receiver_thread,
                   &receiver_args[i]);
  }

  // 启动 sender 线程
  for (int i = 0; i < NUM_SENDERS; i++) {
    sender_args[i].tx = senders[i];
    sender_args[i].thread_id = i;
    sender_args[i].count = MSGS_PER_SENDER;
    pthread_create(&sender_threads[i], NULL, sender_thread, &sender_args[i]);
  }

  // 等待所有 sender 完成
  for (int i = 0; i < NUM_SENDERS; i++) {
    pthread_join(sender_threads[i], NULL);
    chiba_sender_drop(&senders[i]);
  }

  // 等待所有 receiver 完成
  for (int i = 0; i < NUM_RECEIVERS; i++) {
    pthread_join(receiver_threads[i], NULL);
    chiba_receiver_drop(&receivers[i]);
  }

  // 验证
  int final_count = atomic_load_explicit(&total_received, memory_order_relaxed);
  printf("  Total sent: %d, Total received: %d\n", TOTAL_MSGS, final_count);
  assert(final_count == TOTAL_MSGS);

  printf("✓ MPMC bounded channel test passed!\n");
}

int main() {
  printf("=== Chiba Channel Test Suite ===\n\n");

  printf("--- Single-threaded tests ---\n");
  test_unbounded_channel();
  test_bounded_channel();
  test_disconnection();
  test_clone();

  printf("\n--- Multi-threaded tests ---\n");
  test_mpmc_unbounded();
  test_mpmc_bounded();

  printf("\n=== All tests passed! ===\n");
  return 0;
}
