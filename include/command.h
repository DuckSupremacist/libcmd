#pragma once
#include "message.h"

/**
 * @brief Abstract base class representing a command that can be executed
 *
 * This class inherits from ReceivedMessage and defines an interface for executing commands.
 * It requires derived classes to implement the execute() method, which returns a response message.
 *
 * @tparam CommandMessageFormat The format of the command message (received)
 * @tparam ResponseMessageFormat The format of the response message (sent)
 */
template <MessageFormatWithIdFirst CommandMessageFormat, MessageFormatWithIdFirst ResponseMessageFormat>
class Command : public ReceivedMessage<CommandMessageFormat>
{
  public:
    /** @brief Type alias for the input message type */
    using input_message_t = ReceivedMessage<CommandMessageFormat>;
    /** @brief Type alias for the output message type */
    using output_message_t = SentMessage<ResponseMessageFormat>;

    /** @brief Constructs a Command from raw byte input
     *
     * @param content Raw byte content of the command message
     * @throws std::runtime_error if content size is invalid
     **/
    explicit Command(const std::vector<std::uint8_t>& content) : ReceivedMessage<CommandMessageFormat>(content) {}

    /**
     * @brief Executes the command associated with this message
     *
     * @return std::vector<output_message_t> The response message vector after executing the command
     */
    [[nodiscard]] virtual std::vector<std::vector<std::uint8_t>> execute() const = 0;
};
