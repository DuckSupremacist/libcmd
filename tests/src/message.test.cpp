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
};
// POD-like to be safely memcpy'ed
static_assert(std::is_standard_layout_v<GoodFormat>);
static_assert(std::is_trivially_copyable_v<GoodFormat>);
static_assert(offsetof(GoodFormat, id) == 0);

/* ―――――――――――――――― Concept tests ―――――――――――――――― */

// Malformed variants for negative checks (never instantiate Message with them; just concept checks)
struct BadNoId
{
    static constexpr std::uint8_t ID = 1;
    // missing non-static data member `id`
    std::uint8_t a;
};

struct BadIdNotFirst
{
    static constexpr std::uint8_t ID = 2;
    std::uint8_t a;
    std::uint8_t id; // not first
};

struct BadIdWrongType
{
    static constexpr std::uint8_t ID = 3;
    unsigned int id; // not a 1-byte unsigned integral
};

struct BadStaticIdWrongType
{
    static constexpr int ID = 4; // not an UnsignedByte type
    std::uint8_t id;
};

enum class SmallEnum : std::uint8_t
{
    V = 7
};
struct BadStaticIdEnum
{
    static constexpr SmallEnum ID = SmallEnum::V; // enum, not integral type for UnsignedByte
    std::uint8_t id;
};

TEST(Concepts, UnsignedByte) {
    static_assert(UnsignedByte<std::uint8_t>);
    static_assert(!UnsignedByte<std::uint16_t>);
    static_assert(!UnsignedByte<std::int8_t>);
    static_assert(!UnsignedByte<char>); // char may be signed or unsigned, but UnsignedByte requires unsigned & size==1
    SUCCEED();
}

TEST(Concepts, MessageFormatT) {
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

/* ―――――――――――――――― Runtime tests ―――――――――――――――― */

TEST(SentMessageTests, SerializeMatchesStructMemory) {
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

TEST(ReceivedMessageTests, ConstructFromRawBytesRoundTrips) {
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

TEST(ReceivedMessageTests, ThrowsOnWrongSize) {
    // Too small
    const std::vector<std::uint8_t> bad_small(sizeof(GoodFormat) - 1, 0);
    // Too big
    const std::vector<std::uint8_t> bad_big(sizeof(GoodFormat) + 1, 0);

    EXPECT_THROW(ReceivedGoodMessage{bad_small}, std::runtime_error);
    EXPECT_THROW(ReceivedGoodMessage{bad_big}, std::runtime_error);
}

TEST(Polymorphism, BasePointersWork) {
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
