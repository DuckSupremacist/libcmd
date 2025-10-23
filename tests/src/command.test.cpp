#include "command.h"
#include "message.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

/* ―――――――――――――――― Helpers ―――――――――――――――― */

template <typename T> static std::vector<std::uint8_t> serialize(const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    return {reinterpret_cast<const std::uint8_t*>(&obj), reinterpret_cast<const std::uint8_t*>(&obj) + sizeof(T)};
}

template <typename T> static std::vector<std::uint8_t> serialize(const std::uint8_t id, const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    std::vector content{id};
    content.insert(
        content.end(), reinterpret_cast<const std::uint8_t*>(&obj),
        reinterpret_cast<const std::uint8_t*>(&obj) + sizeof(T)
    );
    return content;
}

/* ―――――――――――――――― Formats ―――――――――――――――― */

// Command (input) format: standard-layout, id first, static constexpr std::uint8_t ID
struct CmdFormat
{
    std::uint8_t opcode;
    std::uint16_t param;
};
static_assert(std::is_standard_layout_v<CmdFormat>);
static_assert(std::is_trivially_copyable_v<CmdFormat>);

// Response (output) format
struct RspFormat
{
    std::uint8_t status;
    std::uint16_t value;
};
static_assert(std::is_standard_layout_v<RspFormat>);
static_assert(std::is_trivially_copyable_v<RspFormat>);

/* ―――――――――――――――― Typedefs ―――――――――――――――― */

using TestCommandBase = Command<0x01, CmdFormat>;

/* ―――――――――――――――― Concrete Command for tests ――――――――――――――――
   This command interprets the input and returns one response frame:
   - status = opcode
   - value  = param + 1
*/
class EchoPlusOneCommand final : public TestCommandBase
{
  public:
    using ResponseMessage = SentMessage<ID, RspFormat>;
    explicit EchoPlusOneCommand(const std::vector<std::uint8_t>& raw) : TestCommandBase(raw) {}

    void execute(const Communicator& communicator) const override {
        // Build response payload from input content()
        RspFormat rsp{.status = ID};
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

    [[nodiscard]] REQUEST_STATUS request(
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
    cmd.opcode = 0x33;
    cmd.param = 0x4455;

    const std::vector<std::uint8_t> raw = serialize(EchoPlusOneCommand::ID, cmd);
    const EchoPlusOneCommand command{raw};

    // Upcast checks: public inheritance from ReceivedMessage<CmdFormat>
    [[maybe_unused]] const ReceivedMessage<EchoPlusOneCommand::ID, CmdFormat>* as_received = &command;

    // The stored content equals the original
    EXPECT_EQ(serialize(command.content()), serialize(cmd));
    EXPECT_EQ(command.content().opcode, 0x33);
    EXPECT_EQ(command.content().param, 0x4455);
}

TEST(CommandConstruction, ThrowsOnWrongSize) {
    // Too small
    const std::vector<std::uint8_t> bad_small(sizeof(CmdFormat), 0);
    // Too big
    const std::vector<std::uint8_t> bad_big(sizeof(CmdFormat) + 2, 0);

    EXPECT_THROW(EchoPlusOneCommand{bad_small}, MessageLengthError);
    EXPECT_THROW(EchoPlusOneCommand{bad_big}, MessageLengthError);
}

TEST(CommandExecute, ProducesExpectedResponseBytes) {
    // Arrange input
    CmdFormat cmd{};
    cmd.opcode = 0x7A;
    cmd.param = 0x00FF; // 255

    const std::vector<std::uint8_t> raw = serialize(EchoPlusOneCommand::ID, cmd);
    const EchoPlusOneCommand command{raw};

    // Expected response
    RspFormat expected_rsp{};
    expected_rsp.status = cmd.opcode;                               // echo opcode
    expected_rsp.value = static_cast<std::uint16_t>(cmd.param + 1); // +1

    const EchoPlusOneCommand::ResponseMessage out_msg{expected_rsp};
    const std::vector<std::uint8_t> expected_bytes = out_msg.serialize();

    // Act
    TestCommunicator const comm{};
    command.execute(comm);

    // Assert one frame returned
    ASSERT_EQ(comm.responses.size(), static_cast<std::size_t>(1));
    // Assert content matches expected serialization
    EXPECT_EQ(comm.responses[0], expected_bytes);
}

TEST(CommandExecute, MultipleInstancesIndependentState) {
    // First instance
    CmdFormat command1{};
    command1.opcode = 0x10;
    command1.param = 0x0001;
    const EchoPlusOneCommand cmd1{serialize(EchoPlusOneCommand::ID, command1)};

    // Second instance
    CmdFormat command2{};
    command2.opcode = 0xFE;
    command2.param = 0x00FE;
    const EchoPlusOneCommand cmd2{serialize(EchoPlusOneCommand::ID, command2)};

    // Execute both
    TestCommunicator const comm{};
    cmd1.execute(comm);
    cmd2.execute(comm);

    // Build expected frames
    RspFormat r1{};
    r1.status = command1.opcode;
    r1.value = static_cast<std::uint16_t>(command1.param + 1);

    RspFormat r2{};
    r2.status = command2.opcode;
    r2.value = static_cast<std::uint16_t>(command2.param + 1);

    const std::vector<std::uint8_t> e1 = SentMessage<EchoPlusOneCommand::ID, RspFormat>{r1}.serialize();
    const std::vector<std::uint8_t> e2 = SentMessage<EchoPlusOneCommand::ID, RspFormat>{r2}.serialize();

    ASSERT_EQ(comm.responses.size(), 2);
    EXPECT_EQ(comm.responses[0], e1);
    EXPECT_EQ(comm.responses[1], e2);
}
