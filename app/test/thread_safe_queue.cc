#include "app/Queue.h"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using IntQueue = ThreadSafeQueue<int>;

TEST(ThreadSafeQueueTest, PushAndPopSingleThread) {
    IntQueue q;

    q.push(42);
    q.push(7);

    int a = q.pop();
    int b = q.pop();

    EXPECT_EQ(a, 42);
    EXPECT_EQ(b, 7);
}

TEST(ThreadSafeQueueTest, TryPopEmptyReturnsNullopt) {
    IntQueue q;

    auto v = q.try_pop();
    EXPECT_FALSE(v.has_value());
}

TEST(ThreadSafeQueueTest, BlockingPopUnblocksOnPush) {
    IntQueue q;

    int result = 0;
    std::thread producer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.push(123);
    });

    auto start = std::chrono::system_clock::now();
    int value = q.pop();
    auto end = std::chrono::system_clock::now();

    result = value;

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (producer.joinable()) producer.join();

    EXPECT_EQ(result, 123);
    EXPECT_GE(elapsed_ms, 40);   // should block a bit
    EXPECT_LE(elapsed_ms, 500);  // but not forever
}

TEST(ThreadSafeQueueTest, StopUnblocksPop) {
    IntQueue q;

    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.stop();
    });

    bool threw = false;
    try {
        (void)q.pop();  // should throw when stop() is called
    } catch (const std::runtime_error& e) {
        threw = true;
    }

    if (stopper.joinable()) stopper.join();

    EXPECT_TRUE(threw);
}

TEST(ThreadSafeQueueTest, CommandRequestRoundTrip) {
    ThreadSafeQueue<CommandRequest> q;

    CommandRequest in;
    in.command_name = "join_channel";
    in.payload = R"({"channel_id":"ch-123"})";
    in.conn_id = ConnId{"conn-1"};
    in.user_id = UserId{"user-42"};
    in.current_hub_id = HubId{"hub-9"};
    in.current_channel_id = ChannelId{"ch-old"};
    in.authenticated = true;

    q.push(in);

    CommandRequest out = q.pop();

    EXPECT_EQ(out.command_name, "join_channel");
    EXPECT_EQ(out.payload, R"({"channel_id":"ch-123"})");

    // Compare using .value or another ID object:
    EXPECT_EQ(out.conn_id.value, "conn-1");
    EXPECT_EQ(out.user_id.value, "user-42");
    EXPECT_EQ(out.current_hub_id.value, "hub-9");
    EXPECT_EQ(out.current_channel_id.value, "ch-old");
    EXPECT_TRUE(out.authenticated);
}

TEST(ThreadSafeQueueTest, ProducerConsumerMimicsServerAndWorker) {
    ThreadSafeQueue<CommandRequest> in_q;
    ThreadSafeQueue<OutgoingMessage> out_q;

    constexpr int N = 10;

    // Producer: mimics WS thread pushing incoming messages
    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            CommandRequest req;
            req.command_name = "echo";
            req.payload = "msg-" + std::to_string(i);
            req.conn_id = ConnId{"conn-" + std::to_string(i)};
            req.user_id = UserId{"user-" + std::to_string(i)};
            req.current_hub_id = HubId{"hub-1"};
            req.current_channel_id = ChannelId{"ch-1"};
            req.authenticated = (i % 2 == 0);  // arbitrary

            in_q.push(std::move(req));
        }
    });

    // Consumer: mimics worker/dispatcher handling commands and pushing responses
    std::thread consumer([&] {
        for (int i = 0; i < N; ++i) {
            CommandRequest req = in_q.pop();  // blocking

            // "Dispatcher" logic: just echo back with a prefix
            OutgoingMessage msg;
            msg.conn_id = req.conn_id;
            msg.payload = "handled:" + req.payload;

            out_q.push(std::move(msg));
        }
    });

    // Main thread: read responses and verify
    for (int i = 0; i < N; ++i) {
        OutgoingMessage msg = out_q.pop();  // will block until consumer pushes

        // We don't know the order for sure if we had multiple consumers,
        // but here it's single consumer, single producer, so order matches.
        std::string expected_conn = "conn-" + std::to_string(i);
        std::string expected_payload = "handled:msg-" + std::to_string(i);

        EXPECT_EQ(msg.conn_id.value, expected_conn);
        EXPECT_EQ(msg.payload, expected_payload);
    }

    if (producer.joinable()) producer.join();
    if (consumer.joinable()) consumer.join();
}