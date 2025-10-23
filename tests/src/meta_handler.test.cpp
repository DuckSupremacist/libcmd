#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <string>

#include "meta_handler.h"
#include "communication.h"

/* ―――――――――――――――― Test doubles ―――――――――――――――― */

class DummyCommunicator final : public Communicator {
public:
    void respond(const std::vector<std::uint8_t>& r) const override { last_response = r; ++respond_calls; }

    REQUEST_STATUS request(const std::vector<std::uint8_t>& message,
                           const std::function<void(std::vector<std::uint8_t>)> handle_response_callback) const override {
        last_request = message;
        ++request_calls;
        for (const std::vector<std::uint8_t>& responses : scripted_responses) handle_response_callback(responses);
        return scripted_status;
    }

    // script & introspection
    mutable std::vector<std::uint8_t> last_response{};
    mutable std::vector<std::uint8_t> last_request{};
    mutable int respond_calls = 0;
    mutable int request_calls = 0;
    std::vector<std::vector<std::uint8_t>> scripted_responses{};
    REQUEST_STATUS scripted_status = REQUEST_STATUS::SUCCESS;
};

/* ―――――――――――――――― Fake Handlers ―――――――――――――――― */

struct OkHandlerA {
    static constexpr std::uint8_t ID = 0x01;
    inline static int _calls = 0;

    static HandlerExecuteResult
    execute(const std::vector<std::uint8_t>& data, const Communicator& comm) {
        (void)comm;
        _last_data = data;
        ++_calls;
        return {}; // success
    }

    inline static std::vector<std::uint8_t> _last_data{};
};
static_assert(HandlerLike<OkHandlerA>);

struct OkHandlerB {
    static constexpr std::uint8_t ID = 0x02;
    inline static int _calls = 0;

    static HandlerExecuteResult
    execute(const std::vector<std::uint8_t>& data, const Communicator& comm) {
        (void)comm;
        _last_data = data;
        ++_calls;
        return {}; // success
    }

    inline static std::vector<std::uint8_t> _last_data{};
};
static_assert(HandlerLike<OkHandlerB>);

struct ErrHandler {
    static constexpr std::uint8_t ID = 0x03;
    inline static int _calls = 0;

    static HandlerExecuteResult
    execute(const std::vector<std::uint8_t>&, const Communicator&) {
        ++_calls;
        return unexpected(HandlerExecuteError{
            .code = HANDLER_EXECUTE_STATUS::ERROR_ID_NOT_FOUND,
            .msg  = "boom"
        });
    }
};
static_assert(HandlerLike<ErrHandler>);

// For duplicate-ID compile-time uniqueness helper test
struct Dup1 { static constexpr std::uint8_t ID = 0x7F; static HandlerExecuteResult execute(const std::vector<std::uint8_t>&, const Communicator&) { return {}; } };
struct Dup2 { static constexpr std::uint8_t ID = 0x7F; static HandlerExecuteResult execute(const std::vector<std::uint8_t>&, const Communicator&) { return {}; } };
static_assert(HandlerLike<Dup1> && HandlerLike<Dup2>);

/* ―――――――――――――――― Tests ―――――――――――――――― */

TEST(MetaHandlerConcepts, HandlerLikeAcceptsWellFormed) {
    static_assert(HandlerLike<OkHandlerA>, "OkHandlerA should satisfy HandlerLike");
    static_assert(HandlerLike<OkHandlerB>, "OkHandlerB should satisfy HandlerLike");
    static_assert(HandlerLike<ErrHandler>, "ErrHandler should satisfy HandlerLike");
    SUCCEED();
}

TEST(MetaHandlerHelpers, UniqueIdsDetectsDuplicatesAtCompileTimeValue) {
    // We can't trigger the static_assert in MetaHandler here (it would fail the build),
    // but we can verify the helper trait value is false for duplicate IDs.
    constexpr bool UNIQUE = meta_handler_helpers::UniqueIds<Dup1, Dup2>::value;
    EXPECT_FALSE(UNIQUE);
}

TEST(MetaHandlerExecute, DispatchesToMatchingHandlerByPort) {
    using MH = MetaHandler<OkHandlerA, OkHandlerB>;

    OkHandlerA::_calls = 0;
    OkHandlerB::_calls = 0;
    OkHandlerA::_last_data.clear();
    OkHandlerB::_last_data.clear();

    const DummyCommunicator comm;
    const std::vector<std::uint8_t> payload{0xDE, 0xAD};

    // Call port = OkHandlerB::ID, only B should execute
    const MetaHandlerExecuteResult res = MH::execute(OkHandlerB::ID, payload, comm);

    EXPECT_TRUE(res.ok());
    EXPECT_EQ(OkHandlerA::_calls, 0);
    EXPECT_EQ(OkHandlerB::_calls, 1);
    EXPECT_EQ(OkHandlerB::_last_data, payload);
}

TEST(MetaHandlerExecute, UnknownPortReturnsError) {
    using MH = MetaHandler<OkHandlerA, OkHandlerB>;

    const DummyCommunicator comm;
    const std::vector<std::uint8_t> payload{0x00};

    const MetaHandlerExecuteResult res = MH::execute(0xFF, payload, comm);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error().code, META_HANDLER_EXECUTE_STATUS::ERROR_PORT_NOT_FOUND);
    // message should mention the unknown ID
    EXPECT_NE(res.error().msg.find("Unknown Handler ID"), std::string::npos);
}

TEST(MetaHandlerExecute, PropagatesHandlerErrorAsMetaError) {
    using MH = MetaHandler<OkHandlerA, ErrHandler>;

    ErrHandler::_calls = 0;

    const DummyCommunicator comm;
    const std::vector<std::uint8_t> payload{0xBE, 0xEF};

    const MetaHandlerExecuteResult res = MH::execute(ErrHandler::ID, payload, comm);

    ASSERT_FALSE(res.ok());
    EXPECT_EQ(ErrHandler::_calls, 1);
    // The meta error code should be the cast of the underlying handler error code
    EXPECT_EQ(res.error().code,
              static_cast<META_HANDLER_EXECUTE_STATUS>(HANDLER_EXECUTE_STATUS::ERROR_ID_NOT_FOUND));
    EXPECT_EQ(res.error().msg, "boom");
}
