#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

/* ―――――――――――――――― Exceptions ―――――――――――――――― */
/**
 * @brief Exception thrown when a message has an invalid length
 *
 * This exception is derived from std::length_error and is thrown when
 * the length of a message does not match the expected size.
 */
class MessageLengthError final : public std::length_error
{
  public:
    /**
     * @brief Constructs a MessageLengthError with a specific error message
     * @param what_arg The error message
     */
    explicit MessageLengthError(const std::string& what_arg) : std::length_error(what_arg) {}
    /**
     * @brief Constructs a MessageLengthError with a specific error message
     * @param what_arg The error message
     */
    explicit MessageLengthError(const char* what_arg) : std::length_error(what_arg) {}
};

/**
 * @brief Exception thrown when a message has an incorrect ID
 *
 * This exception is derived from std::invalid_argument and is thrown when
 * the ID of a message does not match the expected ID.
 */
class MessageWrongIdError final : public std::invalid_argument
{
  public:
    /**
     * @brief Constructs a MessageWrongIdError with a specific error message
     * @param what_arg The error message
     */
    explicit MessageWrongIdError(const std::string& what_arg) : std::invalid_argument(what_arg) {}

    /**
     * @brief Constructs a MessageWrongIdError with a specific error message
     * @param what_arg The error message
     */
    explicit MessageWrongIdError(const char* what_arg) : std::invalid_argument(what_arg) {}
};

/* ―――――――――――――――― Classes ―――――――――――――――― */

/**
 * @brief Abstract base class representing a generic message
 *
 * This class provides functionality to serialize and deserialize messages.
 * It serves as a base for both received and sent messages.
 *
 * @tparam MessageFormatT The format of the message content
 */
template <std::uint8_t MessageID, typename MessageFormatT> class Message
{
  protected:
    MessageFormatT _content; ///< Structured content of the message

  public:
    static constexpr std::uint8_t ID = MessageID; ///< ID of the message type

    /** @brief Type alias for the message format */
    using message_format_t = MessageFormatT;

    virtual ~Message() = default;

    /**
     * @brief Constructs a Message from structured content
     *
     * @param content Structured content of the message
     */
    explicit Message(MessageFormatT content) : _content(std::move(content)) {}

    /**
     * @brief Constructs a Message from raw byte input
     *
     * @param content Raw byte content of the message
     * @throws MessageLengthError if content size is invalid
     * @throws MessageWrongIdError if content size is invalid
     */
    explicit Message(const std::vector<std::uint8_t>& content)
        requires std::is_trivially_copyable_v<MessageFormatT>
    {
        if (content.size() != sizeof(MessageFormatT) + sizeof(ID)) {
            throw MessageLengthError(
                "Invalid content size, expected " + std::to_string(sizeof(MessageFormatT) + sizeof(ID)) + ", got " +
                std::to_string(content.size())
            );
        }
        if (content.at(0) != ID) {
            throw MessageWrongIdError(
                "Invalid ID, expected " + std::to_string(ID) + ", got " + std::to_string(content.at(0))
            );
        }
        std::memcpy(&_content, &content[sizeof(ID)], sizeof(MessageFormatT));
    }

    /**
     * @brief Constructs a Message with default content
     * Verifies size and ID.
     * Initializes the `id` field to MessageFormat::ID.
     * May be useful for more complex initialization in derived classes.
     *
     * Usage:
     *     class MyMessage : public Message<MyFormat> {
     *       public:
     *         MyMessage(const std::vector<std::uint8_t>& content) : Message<MyFormat>(std::in_place, content)
     *         { ... }
     *     };
     *
     * @param content Raw byte content of the message
     * @throws MessageLengthError if content size is invalid
     * @throws MessageWrongIdError if content size is invalid
     */
    explicit Message(std::in_place_t, const std::vector<std::uint8_t>& content) {
        if (content.size() != sizeof(MessageFormatT) + sizeof(ID)) {
            throw MessageLengthError(
                "Invalid content size, expected " + std::to_string(sizeof(MessageFormatT) + sizeof(ID)) + ", got " +
                std::to_string(content.size())
            );
        }
        if (content.at(0) != ID) {
            throw MessageWrongIdError(
                "Invalid ID, expected " + std::to_string(ID) + ", got " + std::to_string(content.at(0))
            );
        }
        _content = MessageFormatT{}; // default-initialize all fields
    }

    /**
     * @brief Serializes the message content into a byte vector
     *
     * @return serialized_message_t Serialized byte vector of the message content
     */
    [[nodiscard]] virtual std::vector<std::uint8_t> serialize() const {
        std::vector content{ID};
        content.insert(
            content.end(), reinterpret_cast<const std::uint8_t*>(&_content),
            reinterpret_cast<const std::uint8_t*>(&_content) + sizeof(MessageFormatT)
        );
        return content;
    }

    /**
     * @brief Accessor for the structured content of the message
     *
     * @return const MessageFormat& Reference to the structured content
     */
    [[nodiscard]] const MessageFormatT& content() const { return _content; }
};
