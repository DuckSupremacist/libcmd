#include "command.h"
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

template <typename T> static std::vector<std::uint8_t> toBytes(const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    std::vector<std::uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &obj, sizeof(T));
    return bytes;
}

// -----------------------------------------------------------------------------
// Payloads (packed, trivially copyable)
// -----------------------------------------------------------------------------
struct CmdPayload
{
    static constexpr std::uint8_t ID = 0x10;
    std::uint8_t id;
    std::uint16_t a;
    std::uint32_t b;
} __attribute__((packed));
static_assert(std::is_trivially_copyable_v<CmdPayload>);

struct RspPayload
{
    static constexpr std::uint8_t ID = 0x11;
    std::uint8_t id;
    std::uint16_t a;
    std::uint32_t b;
} __attribute__((packed));
static_assert(std::is_trivially_copyable_v<RspPayload>);

// -----------------------------------------------------------------------------
// A concrete command implementation for testing
// Behavior: response = request with each field incremented by 1
// -----------------------------------------------------------------------------
class PlusOneCommand final : public Command<CmdPayload, RspPayload>
{
  public:
    explicit PlusOneCommand(const std::vector<std::uint8_t>& content) : Command(content) {}

    [[nodiscard]] output_message_t execute() const override {
        return output_message_t(
            RspPayload{
                .id = static_cast<std::uint8_t>(_content.id + 1U),
                .a = static_cast<std::uint16_t>(_content.a + 1U),
                .b = static_cast<std::uint32_t>(_content.b + 1U),
            }
        );
    }
};

// Compile-time check: output_message_t is the expected SentMessage<RspPayload>
static_assert(std::is_same_v<PlusOneCommand::output_message_t, SentMessage<RspPayload>>);

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST(CommandTests, ConstructFromBytesRoundTrip) {
    CmdPayload payload{};
    payload.id = 0x11U;
    payload.a = 0x2233U;
    payload.b = 0x44556677U;

    const std::vector<std::uint8_t> raw = toBytes(payload);
    const PlusOneCommand cmd(raw);

    const CmdPayload& got = cmd.content();
    const std::vector<std::uint8_t> got_bytes = toBytes(got);
    EXPECT_EQ(got_bytes, raw);
}

TEST(CommandTests, ExecuteProducesExpectedResponse) {
    CmdPayload payload{};
    payload.id = 0x01U;
    payload.a = 0x0002U;
    payload.b = 0x00000003U;

    const std::vector<std::uint8_t> raw = toBytes(payload);
    const PlusOneCommand cmd(raw);

    const PlusOneCommand::output_message_t response = cmd.execute();
    const RspPayload& r = response.content();

    EXPECT_EQ(r.id, static_cast<std::uint8_t>(payload.id + 1U));
    EXPECT_EQ(r.a, static_cast<std::uint16_t>(payload.a + 1U));
    EXPECT_EQ(r.b, static_cast<std::uint32_t>(payload.b + 1U));

    const std::vector<std::uint8_t> serialized = response.serialize();
    ASSERT_EQ(serialized.size(), sizeof(RspPayload));

    RspPayload expected{};
    expected.id = static_cast<std::uint8_t>(payload.id + 1U);
    expected.a = static_cast<std::uint16_t>(payload.a + 1U);
    expected.b = static_cast<std::uint32_t>(payload.b + 1U);
    const std::vector<std::uint8_t> expected_bytes = toBytes(expected);

    EXPECT_EQ(serialized, expected_bytes);
}

TEST(CommandTests, ThrowsOnInvalidSize) {
    // Too small
    const std::vector<std::uint8_t> small(1U, 0x00U);
    EXPECT_THROW((PlusOneCommand(small)), std::runtime_error);

    // Too large
    const std::vector<std::uint8_t> large(sizeof(CmdPayload) + 5U, 0xAAU);
    EXPECT_THROW((PlusOneCommand(large)), std::runtime_error);
}

TEST(CommandTests, ResponseSerializeMatchesMemoryImage) {
    CmdPayload payload{};
    payload.id = 0x9AU;
    payload.a = 0x1234U;
    payload.b = 0x89ABCDEFU;

    const std::vector<std::uint8_t> raw = toBytes(payload);
    const PlusOneCommand cmd(raw);

    const PlusOneCommand::output_message_t response = cmd.execute();
    const std::vector<std::uint8_t> bytes = response.serialize();
    ASSERT_EQ(bytes.size(), sizeof(RspPayload));

    RspPayload expected{};
    expected.id = static_cast<std::uint8_t>(payload.id + 1U);
    expected.a = static_cast<std::uint16_t>(payload.a + 1U);
    expected.b = static_cast<std::uint32_t>(payload.b + 1U);

    const std::vector<std::uint8_t> expected_bytes = toBytes(expected);
    EXPECT_EQ(bytes, expected_bytes);
}
