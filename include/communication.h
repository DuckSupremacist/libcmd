#pragma once
#include <cstdint>
#include <functional>
#include <vector>

/**
 * @brief Abstract base class for communication interfaces
 *
 * This class defines the interface for communication mechanisms that can
 */
class Communication
{
  public:
    virtual ~Communication() = default;

    /**
     * @brief Listens for incoming messages and processes them, sending responses as needed
     */
    virtual void listen() = 0;
};

/**
 * @brief This class should be passed to Command::execute() to handle responding and requesting on its own.
 */
class Communicator
{
  public:
    virtual ~Communicator() = default;

    /**
     * @brief Enumeration representing the status of a request operation
     */
    enum class REQUEST_STATUS : std::uint8_t
    {
        SUCCESS = 0,
        ERROR_TIMEOUT = 1,
        ERROR_COMMUNICATION = 2,
        ERROR_UNKNOWN = 3,
    };

    /**
     * @brief Callback function to send a response message for the current request
     */
    std::function<void(const std::vector<std::uint8_t>&)> respond;

    /**
     * @brief Sends a request message and handles each response via a callback
     *
     * @param message The request message to send
     * @param handle_response_callback Callback function to handle each response message
     * @return EXECUTE_STATUS The status of the request execution
     */
    virtual REQUEST_STATUS request(
        const std::vector<std::uint8_t>& message, std::function<void(std::vector<uint8_t>)> handle_response_callback
    ) = 0;

    /**
     * @brief Sends a request message and collects all responses into a vector
     *
     * @param message The request message to send
     * @param[out] responses Vector to store all received response messages
     * @return EXECUTE_STATUS The status of the request execution
     */
    REQUEST_STATUS
    request(const std::vector<std::uint8_t>& message, std::vector<std::vector<std::uint8_t>>& responses) {
        const std::function<void(std::vector<std::uint8_t>)> callback(
            [&responses](const std::vector<std::uint8_t>& raw_response) { responses.push_back(raw_response); }
        );
        return request(message, callback);
    }
};
