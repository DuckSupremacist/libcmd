#pragma once

#include "communication.h"
#include "message.h"

/**
 * @brief Abstract base class representing a command that can be executed
 *
 * This class inherits from ReceivedMessage and defines an interface for
 * executing commands. It requires derived classes to implement the execute()
 * method, which returns a response message.
 *
 * @tparam CommandMessageFormat The format of the command message (received)
 */
template <MessageFormatT CommandMessageFormat> class Command : public ReceivedMessage<CommandMessageFormat>
{
  public:
    /** @brief Type alias for the input message type */
    using input_message_t = ReceivedMessage<CommandMessageFormat>;

    /**
     * @brief Constructs a Command from raw byte input
     *
     * @param content Raw byte content of the command message
     * @throws std::runtime_error if content size is invalid
     */
    explicit Command(const std::vector<std::uint8_t>& content) : ReceivedMessage<CommandMessageFormat>(content) {}

    /**
     * @brief Executes the command associated with this message
     * @param communicator The Communicator instance to handle responses and requests
     */
    virtual void execute(const Communicator& communicator) const = 0;
};
