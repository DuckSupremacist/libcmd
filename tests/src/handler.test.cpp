#include "handler.h"
#include "command.h"
#include "message.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

/* ───────────────────────── Helpers ───────────────────────── */

template <typename T> static std::vector<std::uint8_t> toBytes(const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    std::vector<std::uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &obj, sizeof(T));
    return bytes;
}

/* ───────────────────────── Message Formats ───────────────────────── */

struct FormatA
{
    static constexpr std::uint8_t ID = 0x01;
    std::uint8_t id;   // must be first
    std::uint8_t op;   // payload byte
    std::uint16_t val; // payload word
};
static_assert(std::is_standard_layout_v<FormatA>);
static_assert(std::is_trivially_copyable_v<FormatA>);
static_assert(offsetof(FormatA, id) == 0);
static_assert(MessageFormatT<FormatA>);

struct FormatB
{
    static constexpr std::uint8_t ID = 0x02;
    std::uint8_t id;
    std::uint8_t code;
    std::uint16_t x;
};
static_assert(std::is_standard_layout_v<FormatB>);
static_assert(std::is_trivially_copyable_v<FormatB>);
static_assert(offsetof(FormatB, id) == 0);
static_assert(MessageFormatT<FormatB>);

struct FormatC
{
    static constexpr std::uint8_t ID = 0x03;
    std::uint8_t id;
    std::uint8_t flag;
    std::uint16_t y;
};
static_assert(std::is_standard_layout_v<FormatC>);
static_assert(std::is_trivially_copyable_v<FormatC>);
static_assert(offsetof(FormatC, id) == 0);
static_assert(MessageFormatT<FormatC>);

/* A single response format we’ll use for all Commands. */
struct ResponseFormat
{
    static constexpr std::uint8_t ID = 0x90;
    std::uint8_t id;
    std::uint8_t status;
    std::uint16_t result;
};
static_assert(std::is_standard_layout_v<ResponseFormat>);
static_assert(std::is_trivially_copyable_v<ResponseFormat>);
static_assert(offsetof(ResponseFormat, id) == 0);
static_assert(MessageFormatT<ResponseFormat>);

/* ───────────────────────── Concrete Commands ─────────────────────────
   Each command will increment its respective counter when constructed.
*/

static int constructed_a = 0;
static int constructed_b = 0;
static int constructed_c = 0;

class CommandA final : public Command<FormatA, ResponseFormat>
{
  public:
    explicit CommandA(const std::vector<std::uint8_t>& raw) : Command(raw) { ++constructed_a; } // increment counter

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> execute() const override {
        // Response: status = op, result = val + 1
        ResponseFormat r{};
        r.id = ResponseFormat::ID;
        r.status = this->content().op;
        r.result = static_cast<std::uint16_t>(this->content().val + 1);
        SentMessage<ResponseFormat> s{r};
        std::vector<std::vector<std::uint8_t>> out;
        out.push_back(s.serialize());
        return out;
    }
};

class CommandB final : public Command<FormatB, ResponseFormat>
{
  public:
    explicit CommandB(const std::vector<std::uint8_t>& raw) : Command(raw) { ++constructed_b; } // increment counter

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> execute() const override {
        // Response: status = code, result = x ^ 0x00FF
        ResponseFormat r{};
        r.id = ResponseFormat::ID;
        r.status = this->content().code;
        r.result = static_cast<std::uint16_t>(this->content().x ^ 0x00FFu);
        SentMessage<ResponseFormat> s{r};
        std::vector<std::vector<std::uint8_t>> out;
        out.push_back(s.serialize());
        return out;
    }
};

class CommandC final : public Command<FormatC, ResponseFormat>
{
  public:
    explicit CommandC(const std::vector<std::uint8_t>& raw) : Command(raw) { ++constructed_c; } // increment counter

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> execute() const override {
        // Response: status = flag, result = y
        ResponseFormat r{};
        r.id = ResponseFormat::ID;
        r.status = this->content().flag;
        r.result = static_cast<std::uint16_t>(this->content().y);
        SentMessage<ResponseFormat> s{r};
        std::vector<std::vector<std::uint8_t>> out;
        out.push_back(s.serialize());
        return out;
    }
};

/* ─────────────────── Duplicate-ID Commands ─────────────────── */
namespace
{
struct FormatADuplicate
{
    static constexpr std::uint8_t ID = FormatA::ID; // same as A
    std::uint8_t id;
    std::uint8_t dummy;
    std::uint16_t v;
};
static_assert(std::is_standard_layout_v<FormatADuplicate>);
static_assert(std::is_trivially_copyable_v<FormatADuplicate>);
static_assert(offsetof(FormatADuplicate, id) == 0);
static_assert(MessageFormatT<FormatADuplicate>);

class CommandADuplicate final : public Command<FormatADuplicate, ResponseFormat>
{
  public:
    explicit CommandADuplicate(const std::vector<std::uint8_t>& raw) : Command<FormatADuplicate, ResponseFormat>(raw) {}
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> execute() const override {
        ResponseFormat r{};
        r.id = ResponseFormat::ID;
        r.status = 0;
        r.result = 0;
        SentMessage<ResponseFormat> s{r};
        std::vector<std::vector<std::uint8_t>> out;
        out.push_back(s.serialize());
        return out;
    }
};
} // anonymous namespace

/* ───────────────────────── Concept: CommandLike ───────────────────────── */

TEST(Concepts, CommandLike_PositiveNegative) {
    static_assert(CommandLike<CommandA>, "CommandA should satisfy CommandLike");
    static_assert(CommandLike<CommandB>, "CommandB should satisfy CommandLike");
    static_assert(CommandLike<CommandC>, "CommandC should satisfy CommandLike");

    // A type with the right typedefs but not deriving from Command should fail.
    struct Fake
    {
        using input_message_t [[maybe_unused]] = ReceivedMessage<FormatA>;
        using output_message_t [[maybe_unused]] = SentMessage<ResponseFormat>;
        // Not derived from Command<...>
    };
    static_assert(!CommandLike<Fake>, "Fake must not satisfy CommandLike");
    SUCCEED();
}

/* ───────────────────────── Helpers: CommandId & UniqueIds ───────────────────────── */

TEST(Helpers, CommandIdExtractsStaticID) {
    EXPECT_EQ(command_helpers::cmdId<CommandA>(), FormatA::ID);
    EXPECT_EQ(command_helpers::cmdId<CommandB>(), FormatB::ID);
    EXPECT_EQ(command_helpers::cmdId<CommandC>(), FormatC::ID);
}

TEST(Helpers, UniqueIds_PositiveAndDuplicateDetection) {
    // Distinct IDs => true
    EXPECT_TRUE((command_helpers::UniqueIds<CommandA, CommandB, CommandC>::value));

    // Duplicate IDs => UniqueIds should be false (detected)
    EXPECT_FALSE((command_helpers::UniqueIds<CommandA, CommandADuplicate>::value));

    // Duplicate IDs => UniqueIds should be false (detected) unordered
    EXPECT_FALSE((command_helpers::UniqueIds<CommandA, CommandB, CommandC, CommandADuplicate>::value));
}

/* ───────────────────────── Handler::execute ───────────────────────── */

using TestHandlerABC = Handler<CommandA, CommandB, CommandC>; // A first, B second, C third
using TestHandlerCBA = Handler<CommandC, CommandB, CommandA>; // Different order to prove it’s order-independent

static void resetCounters() {
    constructed_a = 0;
    constructed_b = 0;
    constructed_c = 0;
}

TEST(HandlerExecute, ThrowsOnEmpty) {
    resetCounters();
    const std::vector<std::uint8_t> empty;
    EXPECT_THROW((void)TestHandlerABC::execute(empty), std::runtime_error);
    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 0);
}

TEST(HandlerExecute, ThrowsOnUnknownId) {
    resetCounters();

    const std::uint8_t unknown_id = 0x7F;
    std::vector<std::uint8_t> data;
    data.push_back(unknown_id);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    EXPECT_THROW((void)TestHandlerABC::execute(data), std::runtime_error);
    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 0);
}

TEST(HandlerExecute, DispatchesToMatchingCommandLeftToRight) {
    resetCounters();

    FormatB b{
        .id = FormatB::ID,
        .code = 0x3C,
        .x = 0x0123,
    };

    const std::vector<std::uint8_t> raw_b = toBytes(b);
    const std::vector<std::vector<std::uint8_t>> frames = TestHandlerABC::execute(raw_b);

    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 0);

    ResponseFormat expected{};
    expected.id = ResponseFormat::ID;
    expected.status = b.code;
    expected.result = static_cast<std::uint16_t>(b.x ^ 0x00FFu);

    const std::vector<std::uint8_t> expected_bytes = SentMessage<ResponseFormat>{expected}.serialize();
    ASSERT_EQ(frames.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(frames[0], expected_bytes);
}

TEST(HandlerExecute, DispatchesToMatchingCommandAnyOrder) {
    resetCounters();

    FormatC c{.id = FormatC::ID, .flag = 0xAA, .y = 0xBEEF};

    const std::vector<std::uint8_t> raw_c = toBytes(c);
    const std::vector<std::vector<std::uint8_t>> frames = TestHandlerCBA::execute(raw_c);

    EXPECT_EQ(constructed_a, 0);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 1);

    ResponseFormat expected{};
    expected.id = ResponseFormat::ID;
    expected.status = c.flag;
    expected.result = static_cast<std::uint16_t>(c.y);

    const std::vector<std::uint8_t> expected_bytes = SentMessage<ResponseFormat>{expected}.serialize();
    ASSERT_EQ(frames.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(frames[0], expected_bytes);
}

TEST(HandlerExecute, DispatchesMultipleCommands) {
    resetCounters();

    // CommandA
    FormatA a{.id = FormatA::ID, .op = 0x10, .val = 0x0011};
    const std::vector<std::uint8_t> raw_a = toBytes(a);

    // CommandB
    FormatB b{.id = FormatB::ID, .code = 0x20, .x = 0x0022};
    const std::vector<std::uint8_t> raw_b = toBytes(b);

    // CommandC
    FormatC c{.id = FormatC::ID, .flag = 0x30, .y = 0x0033};
    const std::vector<std::uint8_t> raw_c = toBytes(c);

    // Execute A
    const std::vector<std::vector<std::uint8_t>> frames_a = TestHandlerABC::execute(raw_a);
    EXPECT_EQ(constructed_a, 1);
    EXPECT_EQ(constructed_b, 0);
    EXPECT_EQ(constructed_c, 0);
    ResponseFormat expected_a{};
    expected_a.id = ResponseFormat::ID;
    expected_a.status = a.op;
    expected_a.result = static_cast<std::uint16_t>(a.val + 1);
    const std::vector<std::uint8_t> expected_bytes_a = SentMessage<ResponseFormat>{expected_a}.serialize();
    ASSERT_EQ(frames_a.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(frames_a[0], expected_bytes_a);

    // Execute B
    const std::vector<std::vector<std::uint8_t>> frames_b = TestHandlerABC::execute(raw_b);
    EXPECT_EQ(constructed_a, 1);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 0);
    ResponseFormat expected_b{};
    expected_b.id = ResponseFormat::ID;
    expected_b.status = b.code;
    expected_b.result = static_cast<std::uint16_t>(b.x ^ 0x00FFu);
    const std::vector<std::uint8_t> expected_bytes_b = SentMessage<ResponseFormat>{expected_b}.serialize();
    ASSERT_EQ(frames_b.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(frames_b[0], expected_bytes_b);

    // Execute C
    const std::vector<std::vector<std::uint8_t>> frames_c = TestHandlerABC::execute(raw_c);
    EXPECT_EQ(constructed_a, 1);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 1);
    ResponseFormat expected_c{};
    expected_c.id = ResponseFormat::ID;
    expected_c.status = c.flag;
    expected_c.result = static_cast<std::uint16_t>(c.y);
    const std::vector<std::uint8_t> expected_bytes_c = SentMessage<ResponseFormat>{expected_c}.serialize();
    ASSERT_EQ(frames_c.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(frames_c[0], expected_bytes_c);

    // Execute A
    const std::vector<std::vector<std::uint8_t>> frames_a_bis = TestHandlerABC::execute(raw_a);
    EXPECT_EQ(constructed_a, 2);
    EXPECT_EQ(constructed_b, 1);
    EXPECT_EQ(constructed_c, 1);
    ResponseFormat expected_a_bis{};
    ASSERT_EQ(frames_a_bis.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(frames_a_bis[0], expected_bytes_a);
}
