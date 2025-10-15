#include "command.h"
#include "message.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

/* ―――――――――――――――― Helpers ―――――――――――――――― */

template <typename T> static std::vector<std::uint8_t> toBytes(const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    std::vector<std::uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &obj, sizeof(T));
    return bytes;
}

/* ―――――――――――――――― Formats ―――――――――――――――― */

// Command (input) format: standard-layout, id first, static constexpr std::uint8_t ID
struct CmdFormat
{
    static constexpr std::uint8_t ID = 0x10;
    std::uint8_t id; // must be first
    std::uint8_t opcode;
    std::uint16_t param;
};
static_assert(std::is_standard_layout_v<CmdFormat>);
static_assert(std::is_trivially_copyable_v<CmdFormat>);
static_assert(offsetof(CmdFormat, id) == 0);
static_assert(MessageFormatT<CmdFormat>);

// Response (output) format
struct RspFormat
{
    static constexpr std::uint8_t ID = 0x90;
    std::uint8_t id; // must be first
    std::uint8_t status;
    std::uint16_t value;
};
static_assert(std::is_standard_layout_v<RspFormat>);
static_assert(std::is_trivially_copyable_v<RspFormat>);
static_assert(offsetof(RspFormat, id) == 0);
static_assert(MessageFormatT<RspFormat>);

/* ―――――――――――――――― Typedefs ―――――――――――――――― */

using TestCommandBase = Command<CmdFormat>;
using ResponseMessage = SentMessage<RspFormat>;

/* ―――――――――――――――― Concrete Command for tests ――――――――――――――――
   This command interprets the input and returns one response frame:
   - status = opcode
   - value  = param + 1
*/
class EchoPlusOneCommand final : public TestCommandBase
{
  public:
    explicit EchoPlusOneCommand(const std::vector<std::uint8_t>& raw) : TestCommandBase(raw) {}

    void execute(const Communicator& communicator) const override {
        // Build response payload from input content()
        RspFormat rsp{};
        rsp.id = RspFormat::ID;
        rsp.status = this->content().opcode;
        rsp.value = static_cast<std::uint16_t>(this->content().param + 1);
        communicator.respond(ResponseMessage(rsp).serialize());
    }
};

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

/* ―――――――――――――――― Tests ―――――――――――――――― */

TEST(CommandBasics, IsAbstract) {
    static_assert(std::is_abstract_v<TestCommandBase>, "Command must be abstract");
    SUCCEED();
}

TEST(CommandBasics, TypeAliases) {
    // input_message_t is ReceivedMessage<CmdFormat>
    static_assert(std::is_same_v<TestCommandBase::input_message_t, TestCommandBase::input_message_t>);
    SUCCEED();
}

TEST(CommandConstruction, AcceptsWellFormedRaw) {
    CmdFormat cmd{};
    cmd.id = CmdFormat::ID;
    cmd.opcode = 0x33;
    cmd.param = 0x4455;

    const std::vector<std::uint8_t> raw = toBytes(cmd);
    const EchoPlusOneCommand c{raw};

    // Upcast checks: public inheritance from ReceivedMessage<CmdFormat>
    [[maybe_unused]] const ReceivedMessage<CmdFormat>* as_received = &c;

    // The stored content equals the original
    EXPECT_EQ(toBytes(c.content()), raw);
    EXPECT_EQ(c.content().id, CmdFormat::ID);
    EXPECT_EQ(c.content().opcode, 0x33);
    EXPECT_EQ(c.content().param, 0x4455);
}

TEST(CommandConstruction, ThrowsOnWrongSize) {
    // Too small
    const std::vector<std::uint8_t> bad_small(sizeof(CmdFormat) - 1, 0);
    // Too big
    const std::vector<std::uint8_t> bad_big(sizeof(CmdFormat) + 1, 0);

    EXPECT_THROW(EchoPlusOneCommand{bad_small}, std::runtime_error);
    EXPECT_THROW(EchoPlusOneCommand{bad_big}, std::runtime_error);
}

TEST(CommandExecute, ProducesExpectedResponseBytes) {
    // Arrange input
    CmdFormat cmd{};
    cmd.id = CmdFormat::ID;
    cmd.opcode = 0x7A;
    cmd.param = 0x00FF; // 255

    const std::vector<std::uint8_t> raw = toBytes(cmd);
    const EchoPlusOneCommand c{raw};

    // Expected response
    RspFormat expected_rsp{};
    expected_rsp.id = RspFormat::ID;
    expected_rsp.status = cmd.opcode;                               // echo opcode
    expected_rsp.value = static_cast<std::uint16_t>(cmd.param + 1); // +1

    const ResponseMessage out_msg{expected_rsp};
    const std::vector<std::uint8_t> expected_bytes = out_msg.serialize();

    // Act
    TestCommunicator const comm{};
    c.execute(comm);

    // Assert one frame returned
    ASSERT_EQ(comm.responses.size(), static_cast<std::size_t>(1));
    // Assert content matches expected serialization
    EXPECT_EQ(comm.responses[0], expected_bytes);
}

TEST(CommandExecute, MultipleInstancesIndependentState) {
    // First instance
    CmdFormat c1{};
    c1.id = CmdFormat::ID;
    c1.opcode = 0x10;
    c1.param = 0x0001;
    const EchoPlusOneCommand cmd1{toBytes(c1)};

    // Second instance
    CmdFormat c2{};
    c2.id = CmdFormat::ID;
    c2.opcode = 0xFE;
    c2.param = 0x00FE;
    const EchoPlusOneCommand cmd2{toBytes(c2)};

    // Execute both
    TestCommunicator const comm{};
    cmd1.execute(comm);
    cmd2.execute(comm);

    // Build expected frames
    RspFormat r1{};
    r1.id = RspFormat::ID;
    r1.status = c1.opcode;
    r1.value = static_cast<std::uint16_t>(c1.param + 1);

    RspFormat r2{};
    r2.id = RspFormat::ID;
    r2.status = c2.opcode;
    r2.value = static_cast<std::uint16_t>(c2.param + 1);

    const std::vector<std::uint8_t> e1 = SentMessage{r1}.serialize();
    const std::vector<std::uint8_t> e2 = SentMessage{r2}.serialize();

    ASSERT_EQ(comm.responses.size(), 2);
    EXPECT_EQ(comm.responses[0], e1);
    EXPECT_EQ(comm.responses[1], e2);
}
