#include "message.h"
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

/* ―――――――――――――――― Helpers ―――――――――――――――― */

template <typename T> static std::vector<std::uint8_t> serialize(const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    return {
        reinterpret_cast<const std::uint8_t*>(&obj),
        reinterpret_cast<const std::uint8_t*>(&obj) + sizeof(T)
    };
}

template <typename T> static std::vector<std::uint8_t> serialize(const std::uint8_t id, const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    std::vector content{id};
    content.insert(
        content.end(),
        reinterpret_cast<const std::uint8_t*>(&obj),
        reinterpret_cast<const std::uint8_t*>(&obj) + sizeof(T)
        );
    return content;
}

/* ―――――――――――――――― Formats ―――――――――――――――― */

// A well-formed message format: standard-layout, id is first, static constexpr std::uint8_t ID.
struct GoodFormat
{
    std::uint8_t a;
    std::uint16_t b;
} __attribute__((packed));
// POD-like to be safely memcpy'ed
static_assert(std::is_standard_layout_v<GoodFormat>);
static_assert(std::is_trivially_copyable_v<GoodFormat>);

struct NonTrivialFormat
{
    std::uint16_t len;     // payload length (LE in tests)
    ~NonTrivialFormat() {} // makes it non-trivially copyable // NOLINT(*-use-equals-default)
} __attribute__((packed));
static_assert(std::is_standard_layout_v<NonTrivialFormat>);
static_assert(!std::is_trivially_copyable_v<NonTrivialFormat>);

/* ―――――――――――――――― Concrete Messages for tests ―――――――――――――――― */

using ReceivedGoodMessage = ReceivedMessage<0x01, GoodFormat>;
using SentGoodMessage = SentMessage<0x02, GoodFormat>;

class NonTrivialReceived final : public ReceivedMessage<0x03, NonTrivialFormat>
{
  public:
    using ReceivedMessage::ReceivedMessage; // keep default/inherited ones if needed

    explicit NonTrivialReceived(const std::vector<std::uint8_t>& wire)
        : ReceivedMessage(std::in_place, wire) // validates size>=1 and ID, sets _content.id
    {
        if (wire.size() < 3) {
            throw std::runtime_error("Invalid content size for NonTrivialFormat");
        }
        // Parse 16-bit little-endian length from bytes 1..2
        this->_content.len = static_cast<std::uint16_t>(wire[1] | static_cast<std::uint16_t>(wire[2]) << 8);
    }

    [[nodiscard]] std::vector<std::uint8_t> serialize() const override {
        return {
            ID,
            static_cast<std::uint8_t>(this->_content.len & 0xFF),
            static_cast<std::uint8_t>(this->_content.len >> 8 & 0xFF),
        };
    }
};

/* ―――――――――――――――― Runtime tests ―――――――――――――――― */

TEST(SentMessage, SerializeMatchesStructMemory) {
    // Arrange
    GoodFormat payload{};
    payload.a = 0xAB;
    payload.b = 0xCDEF;

    const SentGoodMessage msg{payload};

    // Act
    const std::vector<std::uint8_t> bytes = msg.serialize();
    const std::vector<std::uint8_t> expected = serialize(SentGoodMessage::ID, payload);

    // Assert
    ASSERT_EQ(bytes.size(), sizeof(GoodFormat) + 1);
    ASSERT_EQ(expected.size(), sizeof(GoodFormat) + 1);
    EXPECT_EQ(bytes, expected) << "Serialized bytes should match raw memory layout";

    // content() should equal payload byte-for-byte
    EXPECT_EQ(serialize(msg.content()), serialize(payload));
    EXPECT_EQ(msg.content().a, 0xAB);
    EXPECT_EQ(msg.content().b, 0xCDEF);
}

TEST(ReceivedMessage, ConstructFromRawBytesRoundTrips) {
    // Arrange: create raw bytes representing a GoodFormat
    GoodFormat original{};
    original.a = 0x11;
    original.b = 0x2233;
    const std::vector<std::uint8_t> raw = serialize(ReceivedGoodMessage::ID, original);

    // Act
    const ReceivedGoodMessage msg{raw};

    // Assert: content equals original (byte-for-byte)
    EXPECT_EQ(serialize(msg.content()), serialize(original));
    EXPECT_EQ(msg.content().a, 0x11);
    EXPECT_EQ(msg.content().b, 0x2233);

    // And serialize() reproduces the same bytes
    EXPECT_EQ(msg.serialize(), raw);
}

TEST(ReceivedMessage, ThrowsOnWrongSize) {
    // Too small
    const std::vector<std::uint8_t> bad_small(sizeof(GoodFormat), 0);
    // Too big
    const std::vector<std::uint8_t> bad_big(sizeof(GoodFormat) + 2, 0);

    EXPECT_THROW(ReceivedGoodMessage{bad_small}, MessageLengthError);
    EXPECT_THROW(ReceivedGoodMessage{bad_big}, MessageLengthError);
}

TEST(ReceivedMessage, ThrowsOnWrongID) {
    // Correct size but wrong ID
    GoodFormat payload{};
    payload.a = 0;
    payload.b = 0;
    const std::vector<std::uint8_t> raw = serialize(ReceivedGoodMessage::ID + 1, payload);

    EXPECT_THROW(ReceivedGoodMessage{raw}, MessageWrongIdError);
}

TEST(NonTrivialMessage, InPlaceCtorParsesLenAndKeepsId) {
    // We won't use serialize() because it would memcpy the non-trivial destructor
    const std::vector<std::uint8_t> wire{NonTrivialReceived::ID, 0x34, 0x12};

    const NonTrivialReceived msg{wire};
    EXPECT_EQ(msg.content().len, 0x1234); // little-endian

    // serialize() should mirror construction
    EXPECT_EQ(msg.serialize(), wire);
}

TEST(NonTrivialMessage, InPlaceCtorRejectsBadId) {
    const std::vector<std::uint8_t> wire_bad_id{NonTrivialReceived::ID + 1, 0x00, 0x00};
    EXPECT_THROW(NonTrivialReceived{wire_bad_id}, MessageWrongIdError);
}

TEST(NonTrivialMessage, InPlaceCtorRejectsTooShort) {
    const std::vector wire_too_short{NonTrivialReceived::ID}; // only ID, no len
    EXPECT_THROW(NonTrivialReceived{wire_too_short}, MessageLengthError);
}

TEST(MessagePolymorphism, BasePointersWork) {
    // Smoke test: ensure proper inheritance and virtual destructor do not crash
    GoodFormat payload{};
    payload.a = 0x55;
    payload.b = 0xAA55;

    constexpr std::uint8_t ID = 0x04;

    const SentMessage<ID, GoodFormat> sm{payload};
    const Message<ID, GoodFormat>* base = &sm;
    const std::vector<std::uint8_t> bytes = base->serialize();
    ASSERT_EQ(bytes.size(), sizeof(GoodFormat) + 1);

    // The first byte should be the ID by contract
    EXPECT_EQ(bytes[0], ID);
}
