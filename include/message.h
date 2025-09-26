#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

/**
 * @brief Abstract base class representing a generic message
 *
 * This class provides functionality to serialize and deserialize messages.
 * It serves as a base for both received and sent messages.
 *
 * @tparam MessageFormat The format of the message content
 */
template <typename MessageFormat> class Message
{
  protected:
    MessageFormat _content; ///< Structured content of the message
    // static_assert(std::is_trivially_copyable_v<MessageFormat>);

    /** @brief Constructs a Message from structured content
     *
     * @param content Structured content of the message
     **/
    explicit Message(MessageFormat content) : _content(std::move(content)) {}

    /** @brief Constructs a Message from raw byte input
     *
     * @param content Raw byte content of the message
     * @throws std::runtime_error if content size is invalid
     **/
    explicit Message(const std::vector<std::uint8_t>& content) {
        if (content.size() != sizeof(MessageFormat)) {
            throw std::runtime_error("Invalid content size");
        }
        std::memcpy(&_content, content.data(), sizeof(MessageFormat));
    }

  public:
    virtual ~Message() = default;

    /** @brief Serializes the message content into a byte vector
     *
     * @return std::vector<std::uint8_t> Serialized byte vector of the message content
     **/
    [[nodiscard]] virtual std::vector<std::uint8_t> serialize() const {
        return {
            reinterpret_cast<const std::uint8_t*>(&_content),
            reinterpret_cast<const std::uint8_t*>(&_content) + sizeof(MessageFormat)
        };
    }

    /** @brief Accessor for the structured content of the message
     *
     * @return const MessageFormat& Reference to the structured content
     **/
    [[nodiscard]] const MessageFormat& content() const { return _content; }
};

/**
 * @brief Class representing a message that has been received
 *
 * This class inherits from Message and is used to represent messages that are received.
 *
 * @tparam ReceivedMessageFormat The format of the received message
 */
template <typename ReceivedMessageFormat> class ReceivedMessage : public Message<ReceivedMessageFormat>
{
  public:
    /** @brief Constructs a ReceivedMessage from raw byte input
     *
     * @param content Raw byte content of the received message
     * @throws std::runtime_error if content size is invalid
     **/
    explicit ReceivedMessage(const std::vector<std::uint8_t>& content) : Message<ReceivedMessageFormat>(content) {}
};

/**
 * @brief Class representing a message that has been sent
 *
 * This class inherits from Message and is used to represent messages that are sent.
 *
 * @tparam SentMessageFormat The format of the sent message
 */
template <typename SentMessageFormat> class SentMessage final : public Message<SentMessageFormat>
{
  public:
    /** @brief Constructs a SentMessage from structured content
     *
     * @param content Structured content of the sent message
     **/
    explicit SentMessage(SentMessageFormat content) : Message<SentMessageFormat>(std::move(content)) {}
};
