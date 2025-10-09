#include "command.h"
#include "handler.h"
#include "message.h"
#include <cstdint>
#include <iomanip>
#include <iostream>

/* ―――――――――――――――― Messages format ―――――――――――――――― */

/**
 * @brief Structure representing the format of a specific received message
 */
struct ReceivedMessageFormat1
{
    static constexpr std::uint8_t ID = 0x01; ///< Unique identifier for this message type
    std::uint8_t id;                         ///< Command identifier
    std::uint8_t arg;                        ///< Some argument associated with the command
} __attribute__((packed));

/**
 * @brief Structure representing the format of a specific received message
 */
struct ReceivedMessageFormat2
{
    static constexpr std::uint8_t ID = 0x02; ///< Unique identifier for this message type
    std::uint8_t id;                         ///< Command identifier
    std::uint8_t arg;                        ///< Some argument associated with the command
} __attribute__((packed));

/**
 * @brief Structure representing the format of a specific received message
 */
struct ReceivedMessageFormat3
{
    static constexpr std::uint8_t ID = 0x03; ///< Unique identifier for this message type
    std::uint8_t id;                         ///< Command identifier
    std::uint8_t arg;                        ///< Some argument associated with the command
} __attribute__((packed));

/**
 * @brief Structure representing the format of a specific sent message
 */
struct SentMessageFormat1
{
    static constexpr std::uint8_t ID = ReceivedMessageFormat1::ID; ///< Unique identifier for this message type
    std::uint8_t id;                                               ///< Command identifier
    std::uint8_t status;                                           ///< Status code
    std::uint32_t value;                                           ///< Some value associated with the command
} __attribute__((packed));

/**
 * @brief Structure representing the format of a specific sent message
 */
struct SentMessageFormat2
{
    static constexpr std::uint8_t ID = ReceivedMessageFormat2::ID; ///< Unique identifier for this message type
    std::uint8_t id;                                               ///< Command identifier
    std::uint8_t status;                                           ///< Status code
    std::uint32_t value;                                           ///< Some value associated with the command
} __attribute__((packed));

/**
 * @brief Structure representing the format of a specific sent message
 */
struct SentMessageFormat3
{
    static constexpr std::uint8_t ID = ReceivedMessageFormat3::ID; ///< Unique identifier for this message type
    std::uint8_t id;                                               ///< Command identifier
    std::uint8_t status;                                           ///< Status code
    std::uint32_t value;                                           ///< Some value associated with the command
} __attribute__((packed));

/* ―――――――――――――――― Messages format ―――――――――――――――― */
using ReceivedMessage1 = ReceivedMessage<ReceivedMessageFormat1>;
using ReceivedMessage2 = ReceivedMessage<ReceivedMessageFormat2>;
using ReceivedMessage3 = ReceivedMessage<ReceivedMessageFormat3>;
using SentMessage1 = SentMessage<SentMessageFormat1>;
using SentMessage2 = SentMessage<SentMessageFormat2>;
using SentMessage3 = SentMessage<SentMessageFormat3>;

/**
 * @brief Class representing a specific command that can be executed
 */
struct Command1 final : Command<ReceivedMessageFormat1, SentMessageFormat1>
{
    /**
     * @brief Constructs a SpecificCommand from raw byte input
     *
     * @param content Raw byte content of the command message
     * @throws std::runtime_error if content size is invalid
     **/
    explicit Command1(const serialized_message_t& content) : Command(content) {}

    /**
     * @brief Executes the command associated with this message
     *
     * @return serialized_message_array_t The response message after
     * executing the command
     */
    [[nodiscard]] serialized_message_array_t execute() const override {
        // Dummy implementation
        return {output_message_t({
                                     .id = this->content().id,
                                     .status = 0x00,
                                     .value = _content.arg * 1U,
                                 })
                    .serialize()};
    }
};

/**
 * @brief Class representing a specific command that can be executed
 */
struct Command2 final : Command<ReceivedMessageFormat2, SentMessageFormat2>
{
    /**
     * @brief Constructs a SpecificCommand from raw byte input
     *
     * @param content Raw byte content of the command message
     * @throws std::runtime_error if content size is invalid
     **/
    explicit Command2(const serialized_message_t& content) : Command(content) {}

    /**
     * @brief Executes the command associated with this message
     *
     * @return serialized_message_array_t The response message after
     * executing the command
     */
    [[nodiscard]] serialized_message_array_t execute() const override {
        // Dummy implementation
        return {output_message_t({
                                     .id = this->content().id,
                                     .status = 0x00,
                                     .value = _content.arg * 2U,
                                 })
                    .serialize()};
    }
};

/**
 * @brief Class representing a specific command that can be executed
 */
struct Command3 final : Command<ReceivedMessageFormat3, SentMessageFormat3>
{
    /**
     * @brief Constructs a SpecificCommand from raw byte input
     *
     * @param content Raw byte content of the command message
     * @throws std::runtime_error if content size is invalid
     **/
    explicit Command3(const serialized_message_t& content) : Command(content) {}

    /**
     * @brief Executes the command associated with this message
     *
     * @return serialized_message_array_t The response message after
     * executing the command
     */
    [[nodiscard]] serialized_message_array_t execute() const override {
        // Dummy implementation
        return {output_message_t({
                                     .id = this->content().id,
                                     .status = 0x00,
                                     .value = _content.arg * 3U,
                                 })
                    .serialize()};
    }
};

using Handler123 = Handler<Command1, Command2, Command3>;

/* ―――――――――――――――― Main ―――――――――――――――― */

static bool isHexChar(const char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isxdigit(uc) != 0;
}

static void printResponse(const serialized_message_t& response) {
    std::cout << "Response: 0x";
    for (const std::uint8_t b : response) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    std::cout << std::dec << std::endl; // reset to decimal
}

int main() {
    std::cout << "Command Handler Test Program" << std::endl;
    std::cout << "Enter 'q' to quit." << std::endl;
    while (true) {
        std::string line;
        std::cout << "Enter hex bytes (contiguous, e.g., 0105): ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        // Quit when received 'q'
        if (line == "q") {
            break;
        }

        // Validate: only hex digits and even length
        bool all_hex = true;
        for (const char i : line) {
            if (!isHexChar(i)) {
                all_hex = false;
                break;
            }
        }
        if (!all_hex) {
            std::cerr << "Error: input must be contiguous hex digits only (0-9, a-f, A-F)." << std::endl;
            continue;
        }
        if (line.size() % 2 != 0) {
            std::cerr << "Error: odd number of hex digits; pad with a leading 0." << std::endl;
            continue;
        }

        // Convert contiguous hex string to bytes
        serialized_message_t data;
        data.reserve(line.size() / 2);
        for (std::size_t i = 0; i < line.size(); i += 2) {
            const std::string byte_str = line.substr(i, 2);
            data.push_back(static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16)));
        }

        // Execute handler
        const Handler123::EXECUTE_STATUS status = Handler123::execute(data, printResponse);
        if (status != Handler123::EXECUTE_STATUS::SUCCESS) {
            std::cerr << "Error: command execution failed with status " << static_cast<int>(status) << std::endl;
        }
        else {
            std::cout << "Command executed successfully." << std::endl;
        }
    }
    return 0;
}
