#include "command.h"
#include "message.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

/* ================= */
/* USER-SPECIFIC MSG */
/* ================= */

/**
 * @brief Structure representing the format of a specific received message
 */
struct SpecificReceivedMessageFormat
{
    static constexpr std::uint8_t ID = 0x01; ///< Unique identifier for this message type
    std::uint8_t id;                         ///< Command identifier
    std::uint8_t arg;                        ///< Some argument associated with the command
} __attribute__((packed));

/**
 * @brief Type alias for a specific received message for example purposes
 */
using SpecificReceivedMessage = ReceivedMessage<SpecificReceivedMessageFormat>;

/**
 * @brief Structure representing the format of a specific sent message
 */
struct SpecificSentMessageFormat
{
    static constexpr std::uint8_t ID = 0x01; ///< Unique identifier for this message type
    std::uint8_t id;                         ///< Command identifier (echoed from received message)
    std::uint8_t status;                     ///< Status code (0x00 = success, others = error codes)
    std::uint32_t value;                     ///< Some value associated with the command
} __attribute__((packed));

/**
 * @brief Type alias for a specific sent message for example purposes
 */
using SpecificSentMessage = SentMessage<SpecificSentMessageFormat>;

/**
 * @brief Class representing a specific command that can be executed
 */
struct SpecificCommand final : Command<SpecificReceivedMessageFormat, SpecificSentMessageFormat>
{
    /**
     * @brief Constructs a SpecificCommand from raw byte input
     *
     * @param content Raw byte content of the command message
     * @throws std::runtime_error if content size is invalid
     **/
    explicit SpecificCommand(const std::vector<std::uint8_t>& content) : Command(content) {}

    /**
     * @brief Executes the command associated with this message
     *
     * @return SentMessage<SpecificSentMessageFormat> The response message after
     * executing the command
     */
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> execute() const override {
        // Dummy implementation
        return {SentMessage<SpecificSentMessageFormat>({
                                                           .id = this->content().id,
                                                           .status = 0x00,
                                                           .value = 0x12345678,
                                                       })
                    .serialize()};
    }
};

/* ===== */
/* USAGE */
/* ===== */
static void printVector(const std::vector<std::uint8_t>& vec) {
    for (const auto& byte : vec) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

int main() {
    // reception
    const std::vector<std::uint8_t> received_data = {0x01, 0x05};
    try {
        const SpecificReceivedMessage recv_msg({0x01, 0x05});
        std::cout << "Received Message:" << std::endl
                  << " - id: " << static_cast<int>(recv_msg.content().id) << std::endl
                  << " - arg: " << static_cast<int>(recv_msg.content().arg) << std::endl;
        std::cout << "Serialized Received Message:\n 0x";
        printVector(recv_msg.serialize());
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Error parsing received message: " << e.what() << std::endl;
        return 1;
    }

    // sending
    constexpr SpecificSentMessageFormat CONTENT = {
        .id = 0x01,
        .status = 0x02,
        .value = 0xDEADBEEF,
    };
    const SpecificSentMessage sent_msg(CONTENT);

    std::cout << "Sent Message:" << std::endl
              << " - id: " << static_cast<int>(sent_msg.content().id) << std::endl
              << " - status: " << static_cast<int>(sent_msg.content().status) << std::endl
              << " - value: " << sent_msg.content().value << std::endl;
    std::cout << "Serialized Sent Message:\n 0x";
    printVector(sent_msg.serialize());

    // command
    try {
        const SpecificCommand cmd(received_data);
        std::cout << "Command Message:" << std::endl
                  << " - id: " << static_cast<int>(cmd.content().id) << std::endl
                  << " - arg: " << static_cast<int>(cmd.content().arg) << std::endl;
        std::cout << "Serialized Command Message:\n 0x";
        printVector(cmd.serialize());

        const std::vector<std::vector<std::uint8_t>> responses = cmd.execute();
        std::cout << "Serialized Command Response Message:\n 0x";
        printVector(responses[0]);
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Error parsing command message: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
