#pragma once
#include "message.h"

/**
 * @brief Concept to ensure a message format has a static ID member
 *
 * This concept checks if a given type T has a static member named ID
 * that is convertible to std::uint8_t. This is used to enforce that
 * message formats used in commands have a unique identifier.
 *
 * @tparam T The type to be checked
 */
template <typename T>
concept MessageFormatWithIdFirst =
    // must have a non-static member `id`
    requires (T& t) { { t.id } -> std::convertible_to<std::uint8_t>; } &&
    // layout precondition for using offsetof
    std::is_standard_layout_v<T> &&
    // id must be the very first member
    requires { requires offsetof(T, id) == 0; };

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
     * @return SentMessage<ResponseMessageFormat> The response message after executing the command
     */
    [[nodiscard]] virtual output_message_t execute() const = 0;
};
