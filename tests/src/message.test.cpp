#include "message.h"
#include "test_utils.h"
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

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

using GoodMessage = Message<0x01, GoodFormat>;

class NonTrivialMessage final : public Message<0x03, NonTrivialFormat>
{
  public:
    using Message::Message; // inherit constructors

    explicit NonTrivialMessage(const std::vector<std::uint8_t>& wire)
        : Message(std::in_place, wire) // validates size>=1 and ID, sets _content.id
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

TEST(Message, SerializeMatchesStructMemory) {
    // Arrange
    GoodFormat payload{};
    payload.a = 0xAB;
    payload.b = 0xCDEF;

    const GoodMessage msg{payload};

    // Act
    const std::vector<std::uint8_t> bytes = msg.serialize();
    const std::vector<std::uint8_t> expected = serialize(GoodMessage::ID, payload);

    // Assert
    ASSERT_EQ(bytes.size(), sizeof(GoodFormat) + 1);
    ASSERT_EQ(expected.size(), sizeof(GoodFormat) + 1);
    EXPECT_EQ(bytes, expected) << "Serialized bytes should match raw memory layout";

    // content() should equal payload byte-for-byte
    EXPECT_EQ(serialize(msg.content()), serialize(payload));
    EXPECT_EQ(msg.content().a, 0xAB);
    EXPECT_EQ(msg.content().b, 0xCDEF);
}

TEST(Message, ConstructFromRawBytesRoundTrips) {
    // Arrange: create raw bytes representing a GoodFormat
    GoodFormat original{};
    original.a = 0x11;
    original.b = 0x2233;
    const std::vector<std::uint8_t> raw = serialize(GoodMessage::ID, original);

    // Act
    const GoodMessage msg{raw};

    // Assert: content equals original (byte-for-byte)
    EXPECT_EQ(serialize(msg.content()), serialize(original));
    EXPECT_EQ(msg.content().a, 0x11);
    EXPECT_EQ(msg.content().b, 0x2233);

    // And serialize() reproduces the same bytes
    EXPECT_EQ(msg.serialize(), raw);
}

TEST(Message, ThrowsOnWrongSize) {
    // Too small
    const std::vector<std::uint8_t> bad_small(sizeof(GoodFormat), 0);
    // Too big
    const std::vector<std::uint8_t> bad_big(sizeof(GoodFormat) + 2, 0);

    EXPECT_THROW(GoodMessage{bad_small}, MessageLengthError);
    EXPECT_THROW(GoodMessage{bad_big}, MessageLengthError);
}

TEST(Message, ThrowsOnWrongID) {
    // Correct size but wrong ID
    GoodFormat payload{};
    payload.a = 0;
    payload.b = 0;
    const std::vector<std::uint8_t> raw = serialize(GoodMessage::ID + 1, payload);

    EXPECT_THROW(GoodMessage{raw}, MessageWrongIdError);
}

TEST(NonTrivialMessage, InPlaceCtorParsesLenAndKeepsId) {
    // We won't use serialize() because it would memcpy the non-trivial destructor
    const std::vector<std::uint8_t> wire{NonTrivialMessage::ID, 0x34, 0x12};

    const NonTrivialMessage msg{wire};
    EXPECT_EQ(msg.content().len, 0x1234); // little-endian

    // serialize() should mirror construction
    EXPECT_EQ(msg.serialize(), wire);
}

TEST(NonTrivialMessage, InPlaceCtorRejectsBadId) {
    const std::vector<std::uint8_t> wire_bad_id{NonTrivialMessage::ID + 1, 0x00, 0x00};
    EXPECT_THROW(NonTrivialMessage{wire_bad_id}, MessageWrongIdError);
}

TEST(NonTrivialMessage, InPlaceCtorRejectsTooShort) {
    const std::vector wire_too_short{NonTrivialMessage::ID}; // only ID, no len
    EXPECT_THROW(NonTrivialMessage{wire_too_short}, MessageLengthError);
}
