#include "command.h"
#include "handler.h"
#include "message.h"
#include <generator>
#include <iomanip>
#include <iostream>

/* ―――――――――――――――― Messages format ―――――――――――――――― */

/**
 * @brief Structure representing the format of a specific received message
 */
struct ReceivedMessageFormat
{
    std::uint8_t arg; ///< Some argument associated with the command
} __attribute__((packed));

/**
 * @brief Structure representing the format of a specific sent message
 */
struct SentMessageFormat
{
    std::uint8_t status; ///< Status code
    std::uint8_t value;  ///< Some value associated with the response
} __attribute__((packed));

/* ―――――――――――――――― Messages format ―――――――――――――――― */

/**
 * @brief Base example command class template for commands with different IDs
 */
template <std::uint8_t CommandID> class CommandX final : public Command<CommandID, ReceivedMessageFormat>
{
    using Base = Command<CommandID, ReceivedMessageFormat>;
    using ResponseMessage = SentMessage<CommandID, SentMessageFormat>;

  public:
    /**
     * @brief Constructs a SpecificCommand from raw byte input
     *
     * @param content Raw byte content of the command message
     * @throws std::runtime_error if content size is invalid
     */
    explicit CommandX(const std::vector<std::uint8_t>& content) : Base(content) {}

    /**
     * @brief Executes the command associated with this message
     * @param communicator The Communicator instance to handle responses and requests
     */
    void execute(const Communicator& communicator) const override {
        // Dummy implementation
        communicator.respond(ResponseMessage({
                                                 .status = 0x00,
                                                 .value = static_cast<std::uint8_t>(this->_content.arg * Base::ID),
                                             })
                                 .serialize());
    }
};

using Command1 = CommandX<0x01>;
using Command2 = CommandX<0x02>;
using Command3 = CommandX<0x03>;

using Handler123 = Handler<0x01, Command1, Command2, Command3>;

/* ―――――――――――――――― Communicator ―――――――――――――――― */
/**
 * @brief Simple Communicator implementation that prints responses to console
 */
class SimpleCommunicator final : public Communicator
{
  public:
    /**
     * @brief Callback function to send a response message for the current request
     * @param response The response message to print
     */
    void respond(const std::vector<uint8_t>& response) const override {
        std::cout << "Response: 0x";
        for (const std::uint8_t b : response) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        std::cout << std::dec << std::endl; // reset to decimal
    }

    /**
     * @brief Sends a request message and handles each response via a callback
     * @return EXECUTE_STATUS The status of the request execution
     */
    [[nodiscard]] REQUEST_STATUS request(
        const std::vector<std::uint8_t>& /*message*/,
        std::function<void(std::vector<std::uint8_t>)> /*handle_response_callback*/
    ) const override {
        // Dummy implementation: no actual request handling
        return REQUEST_STATUS::ERROR_UNKNOWN;
    }
};

/* ―――――――――――――――― Helpers ―――――――――――――――― */

static std::generator<std::vector<std::uint8_t>> inputMessage() {
    std::cout << "Enter 'q' to quit." << std::endl;
    while (true) {
        std::string line;
        std::cout << "Enter hex bytes (contiguous, e.g., 0105): ";
        if (!std::getline(std::cin, line)) {
            exit(1);
        }

        // Quit when received 'q'
        if (line == "q") {
            break;
        }

        // Validate: only hex digits and even length
        bool all_hex = true;
        for (const char i : line) {
            if (!std::isxdigit(static_cast<unsigned char>(i))) {
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
        std::vector<std::uint8_t> data;
        data.reserve(line.size() / 2);
        for (std::size_t i = 0; i < line.size(); i += 2) {
            const std::string byte_str = line.substr(i, 2);
            data.push_back(static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16)));
        }
        co_yield data;
    }
    std::cout << "Exiting." << std::endl;
    co_return;
}

/* ―――――――――――――――― Main ―――――――――――――――― */

int main() {
    std::cout << "Command Handler Test Program" << std::endl;
    for (const std::vector<std::uint8_t>& data : inputMessage()) {
        const SimpleCommunicator communicator;
        // Execute handler
        const Result result = Handler123::execute(data, communicator);

        // Report status
        if (!result) {
            std::cerr << "Error: command execution failed:\n"
                      << "\t- code:\t" << static_cast<int>(result.error().code) << "\t- msg:\t" << result.error().msg
                      << std::endl;
        }
        else {
            std::cout << "Command executed successfully." << std::endl;
        }
    }
    return 0;
}
