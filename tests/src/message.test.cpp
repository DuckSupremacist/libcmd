#include "message.h"
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

// Trivially copyable payload
struct PayloadByteArray
{
    static constexpr std::uint8_t ID = 0x01;
    std::uint8_t id;
    std::uint8_t data[8];
} __attribute__((packed));
static_assert(std::is_trivially_copyable_v<PayloadByteArray>);

// Trivially copyable payload
struct PayloadMixed
{
    static constexpr std::uint8_t ID = 0x02;
    std::uint8_t id;
    std::uint16_t a;
    std::uint32_t b;
} __attribute__((packed));
static_assert(std::is_trivially_copyable_v<PayloadMixed>);

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST(SentMessageTests, SerializeReturnsExactSizeAndMatchesMemory) {
    constexpr PayloadByteArray PAYLOAD{
        .id = 0xDE,
        .data = {0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05},
    };
    const SentMessage<PayloadByteArray> msg(PAYLOAD);

    const std::vector<uint8_t> bytes = msg.serialize();
    ASSERT_EQ(bytes.size(), sizeof(PayloadByteArray));

    const std::vector<uint8_t> expected = toBytes(PAYLOAD);
    EXPECT_EQ(bytes, expected);
}

TEST(ReceivedMessageTests, ConstructFromBytesRoundTrip) {
    PayloadByteArray p{};
    for (std::size_t i = 0; i < 8; ++i)
        p.data[i] = static_cast<std::uint8_t>(i * 7);

    const std::vector<uint8_t> raw = toBytes(p);
    const ReceivedMessage<PayloadByteArray> msg(raw);

    // content() should match original object byte-for-byte
    const std::vector<uint8_t> from_msg = toBytes(msg.content());
    EXPECT_EQ(from_msg, raw);

    // serialize() should produce the same bytes
    const std::vector<uint8_t> serialized = msg.serialize();
    EXPECT_EQ(serialized, raw);
}

TEST(SentMessageTests, ContentAccessorReflectsStoredCopy) {
    PayloadByteArray p{};
    for (int i = 0; i < 8; ++i)
        p.data[i] = static_cast<std::uint8_t>(i);

    const SentMessage msg(p);

    // Mutate original p; msg must keep its own copy
    p.data[0] = 0xFF;

    const PayloadByteArray& c = msg.content();
    EXPECT_EQ(c.data[0], 0x00);
    EXPECT_EQ(c.data[7], 0x07);
}

TEST(ReceivedMessageTests, ThrowsOnInvalidSize) {
    // Too small
    const std::vector<std::uint8_t> small(1, 0x00);
    EXPECT_THROW((ReceivedMessage<PayloadByteArray>(small)), std::runtime_error);

    // Too large
    const std::vector<std::uint8_t> large(sizeof(PayloadByteArray) + 3, 0xAA);
    EXPECT_THROW((ReceivedMessage<PayloadByteArray>(large)), std::runtime_error);
}

TEST(BaseMessageSemantics, SerializeDoesNotAliasReturnedBuffer) {
    PayloadByteArray p{};
    for (int i = 0; i < 8; ++i)
        p.data[i] = static_cast<std::uint8_t>(i + 10);

    const SentMessage msg(p);

    std::vector<uint8_t> bytes = msg.serialize();
    ASSERT_EQ(bytes.size(), sizeof(PayloadByteArray));

    // Mutate the returned vector; the message’s internal state must not change
    bytes[0] ^= 0xFF;

    // Re-serialize and ensure it matches the original payload (not the mutated vector)
    const std::vector<uint8_t> again = msg.serialize();
    const std::vector<uint8_t> expected = toBytes(p);
    EXPECT_EQ(again, expected);
}
