#include "request.h"
#include "communication.h"
#include "result.h"
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

/* ―――――――――――――――― Fake Communicator ―――――――――――――――― */

class DummyCommunicator final : public Communicator
{
  public:
    void respond(const std::vector<std::uint8_t>& data) const override {
        last_response = data;
        respond_calls++;
    }

    REQUEST_STATUS request(
        const std::vector<std::uint8_t>& message,
        const std::function<void(std::vector<std::uint8_t>)> handle_response_callback
    ) const override {
        last_request = message;
        request_calls++;
        for (const std::vector<std::uint8_t>& responses : scripted_responses)
            handle_response_callback(responses);
        return scripted_status;
    }

    // script & introspection
    mutable std::vector<std::uint8_t> last_response{};
    mutable std::vector<std::uint8_t> last_request{};
    mutable int respond_calls = 0;
    mutable int request_calls = 0;
    std::vector<std::vector<std::uint8_t>> scripted_responses{};
    REQUEST_STATUS scripted_status = REQUEST_STATUS::SUCCESS;
};

/* ―――――――――――――――― Request Format & Concrete Request ―――――――――――――――― */

// Simple POD format for the request payload
struct ReqFormat
{
    std::uint8_t x;
    std::uint16_t y;
} __attribute__((packed));
static_assert(std::is_trivially_copyable_v<ReqFormat>);

// Concrete Request: sends the wire, collects all responses, returns their count
class CountResponsesRequest final : public Request<0x31, ReqFormat, std::size_t>
{
    using Base = Request;

  public:
    using Base::Base; // inherit constructors

    [[nodiscard]] ResultT process(const Communicator& communicator) const override {
        // Build wire: [ID][payload-bytes]
        std::vector wire{ID};
        const ReqFormat& fmt = this->_content; // protected in Message, accessible here
        const std::vector<uint8_t> payload = toBytes(fmt);
        wire.insert(wire.end(), payload.begin(), payload.end());

        std::vector<std::vector<std::uint8_t>> responses;
        if (communicator.request(wire, responses) != Communicator::REQUEST_STATUS::SUCCESS) {
            return unexpected<response_t, std::string>(std::string{"request failed"});
        }
        return expected<response_t, std::string>(responses.size());
    }
};

/* ―――――――――――――――― Tests ―――――――――――――――― */

TEST(RequestTests, ProcessSuccessReturnsResponseCount) {
    // Arrange: two scripted responses
    const std::vector<std::uint8_t> r1{0xAA};
    const std::vector<std::uint8_t> r2{0xBB, 0xCC};
    DummyCommunicator comm;
    comm.scripted_responses = {r1, r2};
    comm.scripted_status = Communicator::REQUEST_STATUS::SUCCESS;

    constexpr ReqFormat FMT{.x = 0x12, .y = 0x3456};
    CountResponsesRequest const req(FMT);

    // Act
    const Result res = req.process(comm);

    // Assert
    ASSERT_TRUE(res.ok());
    EXPECT_EQ(res.value(), 2u);

    std::vector expected{CountResponsesRequest::ID};
    const std::vector<uint8_t> payload = toBytes(FMT);
    expected.insert(expected.end(), payload.begin(), payload.end());
    EXPECT_EQ(comm.last_request, expected);
    EXPECT_EQ(comm.request_calls, 1);
}

TEST(RequestTests, ProcessErrorPropagatesAsUnexpected) {
    // Arrange: no responses, timeout status
    DummyCommunicator comm;
    comm.scripted_status = Communicator::REQUEST_STATUS::ERROR_TIMEOUT;

    constexpr ReqFormat FMT{.x = 0x01, .y = 0x0203};
    const CountResponsesRequest req(FMT);

    // Act
    const Result res = req.process(comm);

    // Assert
    ASSERT_FALSE(res.ok());
}

TEST(RequestTests, ConstructFromRawBytesUsesMessageMemcopyCtor) {
    // Arrange: build raw wire [ID][payload]
    constexpr ReqFormat FMT{.x = 0x9A, .y = 0xBEEF};
    std::vector wire{CountResponsesRequest::ID};
    const std::vector<uint8_t> payload = toBytes(FMT);
    wire.insert(wire.end(), payload.begin(), payload.end());

    // Act
    const CountResponsesRequest req(wire);

    // Process with a communicator that returns SUCCESS but no responses
    DummyCommunicator comm;
    comm.scripted_status = Communicator::REQUEST_STATUS::SUCCESS;

    const Result res = req.process(comm);

    // Assert
    ASSERT_TRUE(res.ok());
    EXPECT_EQ(res.value(), 0u);
    EXPECT_EQ(comm.last_request, wire);
}
