#pragma once

#include "communication.h"
#include "message.h"
#include "result.h"
#include <cstdint>

/**
 * @brief Abstract base class representing a request that can be sent out
 *
 * This class inherits from Message and defines an interface for sending out requests.
 * It requires derived classes to implement the process() method, which returns a response.
 *
 * @tparam MessageFormatT The format of the request message
 * @tparam OutputT The type of the response produced by processing the request
 */
template <std::uint8_t RequestID, typename MessageFormatT, typename OutputT> class Request
    : Message<RequestID, MessageFormatT>
{
    using Base = Message<RequestID, MessageFormatT>;

  public:
    /** @brief Type alias for the response type */
    using response_t = OutputT;

    using Base::Base; // inherit constructors

    /**
     * @brief Processes the request and produces a response
     * @param communicator The Communicator instance to handle responses and requests
     * @return Result containing the response or an error
     */
    virtual Result<response_t, std::string> process(const Communicator& communicator) const = 0;
};
