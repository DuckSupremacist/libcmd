#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <vector>

#include "communication.h"

/* ―――――――――――――――― Test Doubles ―――――――――――――――― */

class FakeCommunicator final : public Communicator
{
  public:
    explicit FakeCommunicator(
        const std::vector<std::vector<std::uint8_t>>& scripted_responses = {},
        const REQUEST_STATUS status = REQUEST_STATUS::SUCCESS
    )
        : _scripted_responses(scripted_responses), _scripted_status(status) {}

    // Capture last response passed to respond()
    void respond(const std::vector<std::uint8_t>& raw) const override {
        _last_response = raw;
        ++_respond_calls;
    }

    // Feed scripted responses to the callback; capture request message
    [[nodiscard]] REQUEST_STATUS request(
        const std::vector<std::uint8_t>& message,
        const std::function<void(std::vector<std::uint8_t>)> handle_response_callback
    ) const override {
        _last_request = message;
        ++_request_calls;
        for (const auto& r : _scripted_responses) {
            handle_response_callback(r);
        }
        return _scripted_status;
    }

    using Communicator::request; // bring in overload that collects responses into vector

    // Introspection for tests
    const std::vector<std::uint8_t>& lastResponse() const { return _last_response; }
    const std::vector<std::uint8_t>& lastRequest() const { return _last_request; }
    int requestCalls() const { return _request_calls; }
    int respondCalls() const { return _respond_calls; }

  private:
    std::vector<std::vector<std::uint8_t>> _scripted_responses;
    REQUEST_STATUS _scripted_status;

    mutable std::vector<std::uint8_t> _last_response{};
    mutable std::vector<std::uint8_t> _last_request{};
    mutable int _request_calls = 0;
    mutable int _respond_calls = 0;
};

class FakeListener final : public Listener
{
  public:
    void start() override { _started = true; }
    [[nodiscard]] bool started() const { return _started; }

  private:
    bool _started = false;
};

/* ―――――――――――――――― Tests ―――――――――――――――― */

TEST(CommunicatorTests, RequestWithCallbackInvokesCallbackForEachResponse) {
    // Arrange
    const std::vector<std::uint8_t> req{0x01, 0x02, 0x03};
    const std::vector<std::uint8_t> r1{0x10};
    const std::vector<std::uint8_t> r2{0x20, 0x21};
    const FakeCommunicator comm({r1, r2}, Communicator::REQUEST_STATUS::SUCCESS);

    std::vector<std::vector<std::uint8_t>> seen;
    auto cb = [&](std::vector<std::uint8_t> resp) { seen.push_back(std::move(resp)); };

    // Act
    const auto status = comm.request(req, cb);

    // Assert
    EXPECT_EQ(status, Communicator::REQUEST_STATUS::SUCCESS);
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], r1);
    EXPECT_EQ(seen[1], r2);
    EXPECT_EQ(comm.lastRequest(), req);
    EXPECT_EQ(comm.requestCalls(), 1);
}

TEST(CommunicatorTests, RequestOverloadCollectsAllResponsesInVector) {
    // Arrange
    const std::vector<std::uint8_t> req{0xAA};
    const std::vector<std::uint8_t> r1{0x01};
    const std::vector<std::uint8_t> r2{0x02};
    const std::vector<std::uint8_t> r3{0x03};
    const FakeCommunicator comm({r1, r2, r3}, Communicator::REQUEST_STATUS::ERROR_TIMEOUT);

    std::vector<std::vector<std::uint8_t>> responses;

    // Act
    const auto status = comm.request(req, responses);

    // Assert
    EXPECT_EQ(status, Communicator::REQUEST_STATUS::ERROR_TIMEOUT);
    ASSERT_EQ(responses.size(), 3u);
    EXPECT_EQ(responses[0], r1);
    EXPECT_EQ(responses[1], r2);
    EXPECT_EQ(responses[2], r3);
    EXPECT_EQ(comm.lastRequest(), req);
    EXPECT_EQ(comm.requestCalls(), 1);
}

TEST(CommunicatorTests, RespondIsVirtualAndReceivesBytes) {
    // Arrange
    const FakeCommunicator comm;
    const std::vector<std::uint8_t> reply{0x7E, 0x00, 0x7E};

    // Act
    comm.respond(reply);

    // Assert
    EXPECT_EQ(comm.lastResponse(), reply);
    EXPECT_EQ(comm.respondCalls(), 1);
}

TEST(ListenerTests, StartIsVirtualAndCallable) {
    FakeListener listener;
    EXPECT_FALSE(listener.started());
    listener.start();
    EXPECT_TRUE(listener.started());
}
