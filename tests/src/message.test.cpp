#include "message.h"
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

// A well-formed message format: standard-layout, id is first, static constexpr std::uint8_t ID.
struct GoodFormat
{
    static constexpr std::uint8_t ID = 0x42;
    std::uint8_t id; // must be first
    std::uint8_t a;
    std::uint16_t b;
} __attribute__((packed));
// POD-like to be safely memcpy'ed
static_assert(std::is_standard_layout_v<GoodFormat>);
static_assert(std::is_trivially_copyable_v<GoodFormat>);
static_assert(offsetof(GoodFormat, id) == 0);

struct NonTrivialFormat
{
    static constexpr std::uint8_t ID = 0x77;
    std::uint8_t id;       // must be first
    std::uint16_t len;     // payload length (LE in tests)
    ~NonTrivialFormat() {} // makes it non-trivially copyable // NOLINT(*-use-equals-default)
} __attribute__((packed));
static_assert(std::is_standard_layout_v<NonTrivialFormat>);
static_assert(!std::is_trivially_copyable_v<NonTrivialFormat>);
static_assert(offsetof(NonTrivialFormat, id) == 0);
static_assert(MessageFormatT<NonTrivialFormat>);

/* ―――――――――――――――― Concept tests ―――――――――――――――― */

// Malformed variants for negative checks (never instantiate Message with them; just concept checks)
struct BadNoId
{
    static constexpr std::uint8_t ID = 1;
    // missing non-static data member `id`
    std::uint8_t a;
} __attribute__((packed));

struct BadIdNotFirst
{
    static constexpr std::uint8_t ID = 2;
    std::uint8_t a;
    std::uint8_t id; // not first
} __attribute__((packed));

struct BadIdWrongType
{
    static constexpr std::uint8_t ID = 3;
    unsigned int id; // not a 1-byte unsigned integral
} __attribute__((packed));

struct BadStaticIdWrongType
{
    static constexpr int ID = 4; // not an UnsignedByte type
    std::uint8_t id;
} __attribute__((packed));

enum class SmallEnum : std::uint8_t
{
    V = 7
};
struct BadStaticIdEnum
{
    static constexpr SmallEnum ID = SmallEnum::V; // enum, not integral type for UnsignedByte
    std::uint8_t id;
};

TEST(MessageConcepts, UnsignedByte) {
    static_assert(UnsignedByte<std::uint8_t>);
    static_assert(!UnsignedByte<std::uint16_t>);
    static_assert(!UnsignedByte<std::int8_t>);
    static_assert(!UnsignedByte<char>); // char may be signed or unsigned, but UnsignedByte requires unsigned & size==1
    SUCCEED();
}

TEST(MessageConcepts, MessageFormatT) {
    static_assert(MessageFormatT<GoodFormat>, "GoodFormat should satisfy MessageFormatT");

    static_assert(!MessageFormatT<BadNoId>, "Missing non-static member `id`");
    static_assert(!MessageFormatT<BadIdNotFirst>, "`id` must be first member");
    static_assert(!MessageFormatT<BadIdWrongType>, "`id` must be 1-byte unsigned");
    static_assert(!MessageFormatT<BadStaticIdWrongType>, "T::ID must be 1-byte unsigned integral constant");
    static_assert(!MessageFormatT<BadStaticIdEnum>, "T::ID must satisfy UnsignedByte, not enum");
    SUCCEED();
}

/* ―――――――――――――――― Concrete Messages for tests ―――――――――――――――― */

using ReceivedGoodMessage = ReceivedMessage<GoodFormat>;
using SentGoodMessage = SentMessage<GoodFormat>;

class NonTrivialReceived final : public ReceivedMessage<NonTrivialFormat>
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
            this->_content.id,
            static_cast<std::uint8_t>(this->_content.len & 0xFF),
            static_cast<std::uint8_t>(this->_content.len >> 8 & 0xFF),
        };
    }
};

/* ―――――――――――――――― Runtime tests ―――――――――――――――― */

TEST(SentMessage, SerializeMatchesStructMemory) {
    // Arrange
    GoodFormat payload{};
    payload.id = GoodFormat::ID;
    payload.a = 0xAB;
    payload.b = 0xCDEF;

    const SentGoodMessage msg{payload};

    // Act
    const std::vector<std::uint8_t> bytes = msg.serialize();
    const std::vector<std::uint8_t> expected = toBytes(payload);

    // Assert
    ASSERT_EQ(bytes.size(), sizeof(GoodFormat));
    ASSERT_EQ(expected.size(), sizeof(GoodFormat));
    EXPECT_EQ(bytes, expected) << "Serialized bytes should match raw memory layout";

    // content() should equal payload byte-for-byte
    EXPECT_EQ(toBytes(msg.content()), toBytes(payload));
    EXPECT_EQ(msg.content().id, GoodFormat::ID);
    EXPECT_EQ(msg.content().a, 0xAB);
    EXPECT_EQ(msg.content().b, 0xCDEF);
}

TEST(ReceivedMessage, ConstructFromRawBytesRoundTrips) {
    // Arrange: create raw bytes representing a GoodFormat
    GoodFormat original{};
    original.id = GoodFormat::ID;
    original.a = 0x11;
    original.b = 0x2233;
    const std::vector<std::uint8_t> raw = toBytes(original);

    // Act
    const ReceivedGoodMessage msg{raw};

    // Assert: content equals original (byte-for-byte)
    EXPECT_EQ(toBytes(msg.content()), toBytes(original));
    EXPECT_EQ(msg.content().id, GoodFormat::ID);
    EXPECT_EQ(msg.content().a, 0x11);
    EXPECT_EQ(msg.content().b, 0x2233);

    // And serialize() reproduces the same bytes
    EXPECT_EQ(msg.serialize(), raw);
}

TEST(ReceivedMessage, ThrowsOnWrongSize) {
    // Too small
    const std::vector<std::uint8_t> bad_small(sizeof(GoodFormat) - 1, 0);
    // Too big
    const std::vector<std::uint8_t> bad_big(sizeof(GoodFormat) + 1, 0);

    EXPECT_THROW(ReceivedGoodMessage{bad_small}, MessageLengthError);
    EXPECT_THROW(ReceivedGoodMessage{bad_big}, MessageLengthError);
}

TEST(ReceivedMessage, ThrowsOnWrongID) {
    // Correct size but wrong ID
    GoodFormat bad_id{};
    bad_id.id = GoodFormat::ID + 1; // wrong ID
    bad_id.a = 0;
    bad_id.b = 0;
    const std::vector<std::uint8_t> raw = toBytes(bad_id);

    EXPECT_THROW(ReceivedGoodMessage{raw}, MessageWrongIdError);
}

TEST(NonTrivialMessage, InPlaceCtorParsesLenAndKeepsId) {
    // We won't use toBytes() because it would memcpy the non-trivial destructor
    const std::vector<std::uint8_t> wire{NonTrivialFormat::ID, 0x34, 0x12};

    const NonTrivialReceived msg{wire};
    EXPECT_EQ(msg.content().id, NonTrivialFormat::ID);
    EXPECT_EQ(msg.content().len, 0x1234); // little-endian

    // serialize() should mirror construction
    EXPECT_EQ(msg.serialize(), wire);
}

TEST(NonTrivialMessage, InPlaceCtorRejectsBadId) {
    const std::vector<std::uint8_t> wire_bad_id{static_cast<std::uint8_t>(NonTrivialFormat::ID + 1), 0x00, 0x00};
    EXPECT_THROW(NonTrivialReceived{wire_bad_id}, MessageWrongIdError);
}

TEST(NonTrivialMessage, InPlaceCtorRejectsTooShort) {
    const std::vector wire_too_short{NonTrivialFormat::ID}; // only ID, no len
    EXPECT_THROW(NonTrivialReceived{wire_too_short}, MessageLengthError);
}

TEST(MessagePolymorphism, BasePointersWork) {
    // Smoke test: ensure proper inheritance and virtual destructor do not crash
    GoodFormat payload{};
    payload.id = GoodFormat::ID;
    payload.a = 0x55;
    payload.b = 0xAA55;

    const SentMessage sm{payload};
    const Message<GoodFormat>* base = &sm;
    const std::vector<std::uint8_t> bytes = base->serialize();
    ASSERT_EQ(bytes.size(), sizeof(GoodFormat));

    // The first byte should be the ID by contract
    EXPECT_EQ(bytes[0], GoodFormat::ID);
}
