#pragma once

#include "communication.h"
#include "message.h"
#include "result.h"
#include <cstdint>

template <std::uint8_t RequestID, typename MessageFormatT, typename OutputT> class Request
    : Message<RequestID, MessageFormatT>
{
    using Base = Message<RequestID, MessageFormatT>;

  public:
    using response_t = OutputT;

    using Base::Base; // inherit constructors

    /**
     * @brief Processes the request and produces a response
     * @param communicator The Communicator instance to handle responses and requests
     * @return Result containing the response or an error
     */
    virtual Result<response_t, std::string> process(const Communicator& communicator) const = 0;
};
