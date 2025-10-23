#include "handler.h"
#include "command.h"
#include "message.h"
#include "test_utils.h"
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

/* ───────────────────────── Message Formats ───────────────────────── */

struct FormatA
{
    std::uint8_t op;   // payload byte
    std::uint16_t val; // payload word
};
static_assert(std::is_standard_layout_v<FormatA>);
static_assert(std::is_trivially_copyable_v<FormatA>);

struct FormatB
{
    std::uint8_t code;
    std::uint16_t x;
};
static_assert(std::is_standard_layout_v<FormatB>);
static_assert(std::is_trivially_copyable_v<FormatB>);

struct FormatC
{
    std::uint8_t flag;
    std::uint16_t y;
};
static_assert(std::is_standard_layout_v<FormatC>);
static_assert(std::is_trivially_copyable_v<FormatC>);

/* A single response format we’ll use for all Commands. */
struct ResponseFormat
{
    std::uint8_t status;
    std::uint16_t result;
};
static_assert(std::is_standard_layout_v<ResponseFormat>);
static_assert(std::is_trivially_copyable_v<ResponseFormat>);

/* ───────────────────────── Concrete Commands ─────────────────────────
   Each command will increment its respective counter when constructed.
*/

static int constructed_a = 0;
static int constructed_b = 0;
static int constructed_c = 0;

class CommandA final : public Command<0x0a, FormatA>
{
  public:
    using ResponseMessage = Message<ID, ResponseFormat>;

    explicit CommandA(const std::vector<std::uint8_t>& raw) : Command(raw) { ++constructed_a; } // increment counter

    void execute(const Communicator& communicator) const override {
        // Response: status = op, result = val + 1
        ResponseFormat r{};
        r.status = this->content().op;
        r.result = static_cast<std::uint16_t>(this->content().val + 1);
        communicator.respond(ResponseMessage(r).serialize());
    }
};

class CommandB final : public Command<0x0b, FormatB>
{
  public:
    using ResponseMessage = Message<ID, ResponseFormat>;

    explicit CommandB(const std::vector<std::uint8_t>& raw) : Command(raw) { ++constructed_b; } // increment counter

    void execute(const Communicator& communicator) const override {
        // Response: status = code, result = x ^ 0x00FF
        ResponseFormat r{};
        r.status = this->content().code;
        r.result = static_cast<std::uint16_t>(this->content().x ^ 0x00FFu);
        communicator.respond(ResponseMessage(r).serialize());
    }
};

class CommandC final : public Command<0x0c, FormatC>
{
  public:
    using ResponseMessage = Message<ID, ResponseFormat>;

    explicit CommandC(const std::vector<std::uint8_t>& raw) : Command(raw) { ++constructed_c; } // increment counter

    void execute(const Communicator& communicator) const override {
        // Response: status = flag, result = y
        ResponseFormat r{};
        r.status = this->content().flag;
        r.result = static_cast<std::uint16_t>(this->content().y);
        communicator.respond(ResponseMessage(r).serialize());
    }
};

/* ─────────────────── Duplicate-ID Commands ─────────────────── */

class CommandADuplicate final : public Command<CommandA::ID, FormatA>
{
  public:
    using ResponseMessage = Message<ID, ResponseFormat>;
    using Command::Command; // inherit constructors

    void execute(const Communicator& communicator) const override {
        ResponseFormat r{};
        r.status = 0;
        r.result = 0;
        communicator.respond(ResponseMessage(r).serialize());
    }
};

/* ───────────────────────── Concept: CommandLike ───────────────────────── */

// A type that satisfies CommandLike but is not derived from Command should pass.
class GoodCommandA final
{
  public:
    const std::uint8_t ID = 0x0a;
    explicit GoodCommandA([[maybe_unused]] const std::vector<std::uint8_t>& raw) {}
    void execute([[maybe_unused]] const Communicator& communicator) const {}
};

// A type that does not have an ID should fail.
class BadIDCommand final
{
  public:
    explicit BadIDCommand([[maybe_unused]] const std::vector<std::uint8_t>& raw) {}
    void execute([[maybe_unused]] const Communicator& communicator) const {}
};
static_assert(!CommandLike<BadIDCommand>, "BadIDCommand should not satisfy CommandLike");

// A type that does not have an execute method should fail.
class BadExecuteCommand final
{
  public:
    const std::uint8_t ID = 0x0a;
    explicit BadExecuteCommand([[maybe_unused]] const std::vector<std::uint8_t>& raw) {}
};
static_assert(!CommandLike<BadExecuteCommand>, "BadExecuteCommand should not satisfy CommandLike");

// A type that does not have the correct constructor should fail.
class BadConstructorCommand final
{
  public:
    static constexpr std::uint8_t ID = 0x0a;
    BadConstructorCommand() = default;
    void execute([[maybe_unused]] const Communicator& communicator) const {}
};

TEST(HandlerConcepts, CommandLike) {
    static_assert(CommandLike<CommandA>, "CommandA should satisfy CommandLike");
    static_assert(CommandLike<CommandB>, "CommandB should satisfy CommandLike");
    static_assert(CommandLike<CommandC>, "CommandC should satisfy CommandLike");
    static_assert(CommandLike<GoodCommandA>, "GoodCommandA should satisfy CommandLike");

    static_assert(!CommandLike<BadIDCommand>, "BadIDCommand should not satisfy CommandLike");
    static_assert(!CommandLike<BadExecuteCommand>, "BadExecuteCommand should not satisfy CommandLike");
    static_assert(!CommandLike<BadConstructorCommand>, "BadConstructorCommand should not satisfy CommandLike");

    SUCCEED();
}

/* ───────────────────────── Helpers: CommandId & UniqueIds ───────────────────────── */

TEST(Helpers, UniqueIdsDetection) {
    // Distinct IDs => true
    static_assert(command_helpers::UniqueIds<CommandA, CommandB, CommandC>::value);

    // Duplicate IDs => UniqueIds should be false (detected)
    static_assert(!command_helpers::UniqueIds<CommandA, CommandADuplicate>::value);

    // Duplicate IDs => UniqueIds should be false (detected) unordered
    static_assert(!command_helpers::UniqueIds<CommandB, CommandA, CommandC, CommandADuplicate>::value);

    SUCCEED();
}

/* ───────────────────────── Handler::execute ───────────────────────── */

using TestHandlerABC = Handler<0x01, CommandA, CommandB, CommandC>; // A first, B second, C third
using TestHandlerCBA = Handler<0x02, CommandC, CommandB, CommandA>; // Different order to prove it’s order-independent

class TestCommunicator final : public Communicator
{
  public:
    mutable std::vector<std::vector<std::uint8_t>> responses;

    void respond(const std::vector<std::uint8_t>& response) const override { responses.push_back(response); }

    REQUEST_STATUS request(
        const std::vector<std::uint8_t>& message, std::function<void(std::vector<uint8_t>)> handle_response_callback
    ) const override {
        GTEST_NONFATAL_FAILURE_("Not implemented in this test");
        return REQUEST_STATUS::ERROR_UNKNOWN;
    }
};

static void resetCounters() {
    constructed_a = 0;
    constructed_b = 0;
    constructed_c = 0;
}

TEST(HandlerID, IsCorrect) {
    static_assert(TestHandlerABC::ID == 0x01);
    static_assert(TestHandlerCBA::ID == 0x02);
    SUCCEED();
}

TEST(HandlerExecute, ThrowsOnEmpty) {
    resetCounters();
    const std::vector<std::uint8_t> empty;
    const TestCommunicator communicator;
    const Result result = TestHandlerABC::execute(empty, communicator);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, HANDLER_EXECUTE_STATUS::ERROR_EMPTY_MESSAGE);
    EXPECT_EQ(communicator.responses.size(), 0);
    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 0);
}

TEST(HandlerExecute, ThrowsOnUnknownId) {
    resetCounters();

    constexpr std::uint8_t UNKNOWN_ID = 0x7F;
    std::vector<std::uint8_t> data;
    data.push_back(UNKNOWN_ID);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    const TestCommunicator communicator;
    const Result result = TestHandlerABC::execute(data, communicator);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, HANDLER_EXECUTE_STATUS::ERROR_ID_NOT_FOUND);
    EXPECT_EQ(communicator.responses.size(), 0);
    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 0);
}

TEST(HandlerExecute, ThrowsOnWrongSize) {
    resetCounters();

    std::vector<std::uint8_t> data;
    data.push_back(CommandA::ID);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00); // too long
    const TestCommunicator communicator;
    const Result result = TestHandlerABC::execute(data, communicator);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, HANDLER_EXECUTE_STATUS::ERROR_MESSAGE_LENGTH_ERROR);
    EXPECT_EQ(communicator.responses.size(), 0);
    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 0);
}

TEST(HandlerExecute, DispatchesToMatchingCommandLeftToRight) {
    resetCounters();

    constexpr FormatB B{
        .code = 0x3C,
        .x = 0x0123,
    };

    const std::vector<std::uint8_t> raw_b = serialize(CommandB::ID, B);

    const TestCommunicator communicator;
    const Result result = TestHandlerABC::execute(raw_b, communicator);
    ASSERT_TRUE(result);

    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 0);

    ResponseFormat expected{};
    expected.status = B.code;
    expected.result = static_cast<std::uint16_t>(B.x ^ 0x00FFu);

    const std::vector<std::uint8_t> expected_bytes = CommandB::ResponseMessage{expected}.serialize();
    EXPECT_EQ(communicator.responses.size(), 1);
    EXPECT_EQ(communicator.responses[0], expected_bytes);
}

TEST(HandlerExecute, DispatchesToMatchingCommandAnyOrder) {
    resetCounters();

    constexpr FormatC C{.flag = 0xAA, .y = 0xBEEF};

    const std::vector<std::uint8_t> raw_c = serialize(CommandC::ID, C);

    const TestCommunicator communicator;
    const Result result = TestHandlerABC::execute(raw_c, communicator);
    ASSERT_TRUE(result);

    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 1);

    ResponseFormat expected{};
    expected.status = C.flag;
    expected.result = C.y;

    const std::vector<std::uint8_t> expected_bytes = CommandC::ResponseMessage{expected}.serialize();
    EXPECT_EQ(communicator.responses.size(), 1);
    EXPECT_EQ(communicator.responses[0], expected_bytes);
}

TEST(HandlerExecute, DispatchesMultipleCommands) {
    resetCounters();

    // CommandA
    constexpr FormatA A{.op = 0x10, .val = 0x0011};
    const std::vector<std::uint8_t> raw_a = serialize(CommandA::ID, A);

    // CommandB
    constexpr FormatB B{.code = 0x20, .x = 0x0022};
    const std::vector<std::uint8_t> raw_b = serialize(CommandB::ID, B);

    // CommandC
    constexpr FormatC C{.flag = 0x30, .y = 0x0033};
    const std::vector<std::uint8_t> raw_c = serialize(CommandC::ID, C);

    // Execute A
    const TestCommunicator communicator;
    const Result result_a = TestHandlerABC::execute(raw_a, communicator);
    ASSERT_TRUE(result_a);
    EXPECT_EQ(constructed_a, 1);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 0);
    ResponseFormat expected_a{};
    expected_a.status = A.op;
    expected_a.result = static_cast<std::uint16_t>(A.val + 1);
    const std::vector<std::uint8_t> expected_bytes_a = CommandA::ResponseMessage{expected_a}.serialize();
    EXPECT_EQ(communicator.responses.size(), 1);
    EXPECT_EQ(communicator.responses[0], expected_bytes_a);

    // Execute B
    communicator.responses.clear();
    const Result result_b = TestHandlerABC::execute(raw_b, communicator);
    ASSERT_TRUE(result_b);
    EXPECT_EQ(constructed_a, 1);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 0);
    ResponseFormat expected_b{};
    expected_b.status = B.code;
    expected_b.result = static_cast<std::uint16_t>(B.x ^ 0x00FFu);
    const std::vector<std::uint8_t> expected_bytes_b = CommandB::ResponseMessage{expected_b}.serialize();
    EXPECT_EQ(communicator.responses.size(), 1);
    EXPECT_EQ(communicator.responses[0], expected_bytes_b);

    // Execute C
    communicator.responses.clear();
    const Result result_c = TestHandlerABC::execute(raw_c, communicator);
    ASSERT_TRUE(result_c);
    EXPECT_EQ(constructed_a, 1);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 1);
    ResponseFormat expected_c{};
    expected_c.status = C.flag;
    expected_c.result = C.y;
    const std::vector<std::uint8_t> expected_bytes_c = CommandC::ResponseMessage{expected_c}.serialize();
    EXPECT_EQ(communicator.responses.size(), 1);
    EXPECT_EQ(communicator.responses[0], expected_bytes_c);

    // Execute A
    communicator.responses.clear();
    const Result result_d = TestHandlerABC::execute(raw_a, communicator);
    ASSERT_TRUE(result_d);
    EXPECT_EQ(constructed_a, 2);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 1);
    EXPECT_EQ(communicator.responses.size(), 1);
    EXPECT_EQ(communicator.responses[0], expected_bytes_a);
}
