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
 * @tparam MessageFormat The format of the command message (received)
 */
template <std::uint8_t CommandID, typename MessageFormat> class Command : public Message<CommandID, MessageFormat>
{
    using Base = Message<CommandID, MessageFormat>;

  public:
    /** @brief Type alias for the input message type */
    using input_message_t = Base;

    using Base::Base; // inherit constructors

    /**
     * @brief Executes the command associated with this message
     * @param communicator The Communicator instance to handle responses and requests
     */
    virtual void execute(const Communicator& communicator) const = 0;
};
