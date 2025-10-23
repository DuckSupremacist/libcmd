#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "communication.h"
#include "meta_handler.h"

/* ―――――――――――――――― Test doubles ―――――――――――――――― */

class DummyCommunicator final : public Communicator
{
  public:
    void respond(const std::vector<std::uint8_t>& data) const override {
        last_response = data;
        ++respond_calls;
    }

    REQUEST_STATUS request(
        const std::vector<std::uint8_t>& message,
        const std::function<void(std::vector<std::uint8_t>)> handle_response_callback
    ) const override {
        last_request = message;
        ++request_calls;
        for (const std::vector<std::uint8_t>& responses : scripted_responses)
            handle_response_callback(responses);
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

struct OkHandlerA
{
    static constexpr std::uint8_t ID = 0x01;
    inline static int _calls = 0;

    static HandlerExecuteResult execute(const std::vector<std::uint8_t>& data, const Communicator& comm) {
        (void)comm;
        _last_data = data;
        ++_calls;
        return {}; // success
    }

    inline static std::vector<std::uint8_t> _last_data{};
};

struct OkHandlerB
{
    static constexpr std::uint8_t ID = 0x02;
    inline static int _calls = 0;

    static HandlerExecuteResult execute(const std::vector<std::uint8_t>& data, const Communicator& comm) {
        (void)comm;
        _last_data = data;
        ++_calls;
        return {}; // success
    }

    inline static std::vector<std::uint8_t> _last_data{};
};

struct ErrHandler
{
    static constexpr std::uint8_t ID = 0x03;
    inline static int _calls = 0;

    static HandlerExecuteResult execute(const std::vector<std::uint8_t>&, const Communicator&) {
        ++_calls;
        return unexpected(HandlerExecuteError{.code = HANDLER_EXECUTE_STATUS::ERROR_ID_NOT_FOUND, .msg = "boom"});
    }
};

struct BadHandlerNoID
{
    static HandlerExecuteResult execute(const std::vector<std::uint8_t>&, const Communicator&) { return {}; }
};

struct BadHandlerBadExecute
{
    static constexpr std::uint8_t ID = 0x04;
    // Missing execute method
};

// For duplicate-ID compile-time uniqueness helper test
struct Dup1
{
    static constexpr std::uint8_t ID = 0x7F;
    static HandlerExecuteResult execute(const std::vector<std::uint8_t>&, const Communicator&) { return {}; }
};
struct Dup2
{
    static constexpr std::uint8_t ID = 0x7F;
    static HandlerExecuteResult execute(const std::vector<std::uint8_t>&, const Communicator&) { return {}; }
};

TEST(HandlerLikeConcept, DetectIsHandlerLike) {
    static_assert(HandlerLike<OkHandlerA>, "OkHandlerA should satisfy HandlerLike");
    static_assert(HandlerLike<OkHandlerB>, "OkHandlerB should satisfy HandlerLike");
    static_assert(HandlerLike<ErrHandler>, "ErrHandler should satisfy HandlerLike");
    static_assert(HandlerLike<Dup1>, "Dup1 should satisfy HandlerLike");
    static_assert(HandlerLike<Dup2>, "Dup2 should satisfy HandlerLike");
    static_assert(HandlerLike<ErrHandler>, "ErrHandler should satisfy HandlerLike");

    static_assert(!HandlerLike<BadHandlerNoID>, "BadHandlerNoID should not satisfy HandlerLike");
    static_assert(!HandlerLike<BadHandlerBadExecute>, "BadHandlerBadExecute should not satisfy HandlerLike");
    SUCCEED();
}
TEST(HandlerLikeConcept, DetectsDuplicateIDsAtCompileTime) {
    static_assert(meta_handler_helpers::UniqueIds<OkHandlerA, OkHandlerB>::value, "IDs should be unique");
    static_assert(!meta_handler_helpers::UniqueIds<Dup1, Dup2>::value, "IDs should not be unique");
    SUCCEED();
}

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
    using TestMetaHandler = MetaHandler<OkHandlerA, OkHandlerB>;

    OkHandlerA::_calls = 0;
    OkHandlerB::_calls = 0;
    OkHandlerA::_last_data.clear();
    OkHandlerB::_last_data.clear();

    const DummyCommunicator comm;
    const std::vector<std::uint8_t> payload{0xDE, 0xAD};

    // Call port = OkHandlerB::ID, only B should execute
    const MetaHandlerExecuteResult res = TestMetaHandler::execute(OkHandlerB::ID, payload, comm);

    EXPECT_TRUE(res.ok());
    EXPECT_EQ(OkHandlerA::_calls, 0);
    EXPECT_EQ(OkHandlerB::_calls, 1);
    EXPECT_EQ(OkHandlerB::_last_data, payload);
}

TEST(MetaHandlerExecute, UnknownPortReturnsError) {
    using TestMetaHandler = MetaHandler<OkHandlerA, OkHandlerB>;

    const DummyCommunicator comm;
    const std::vector<std::uint8_t> payload{0x00};

    const MetaHandlerExecuteResult res = TestMetaHandler::execute(0xFF, payload, comm);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error().code, META_HANDLER_EXECUTE_STATUS::ERROR_PORT_NOT_FOUND);
    // message should mention the unknown ID
    EXPECT_NE(res.error().msg.find("Unknown Handler ID"), std::string::npos);
}

TEST(MetaHandlerExecute, PropagatesHandlerErrorAsMetaError) {
    using TestMetaHandler = MetaHandler<OkHandlerA, ErrHandler>;

    ErrHandler::_calls = 0;

    const DummyCommunicator comm;
    const std::vector<std::uint8_t> payload{0xBE, 0xEF};

    const MetaHandlerExecuteResult res = TestMetaHandler::execute(ErrHandler::ID, payload, comm);

    ASSERT_FALSE(res.ok());
    EXPECT_EQ(ErrHandler::_calls, 1);
    // The meta error code should be the cast of the underlying handler error code
    EXPECT_EQ(res.error().code, static_cast<META_HANDLER_EXECUTE_STATUS>(HANDLER_EXECUTE_STATUS::ERROR_ID_NOT_FOUND));
    EXPECT_EQ(res.error().msg, "boom");
}
