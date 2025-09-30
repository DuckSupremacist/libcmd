#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

/* ―――――――――――――――― Concepts ―――――――――――――――― */

/**
 * @brief Helper concept to ensure a type is an unsigned byte (std::uint8_t, unsigned char, etc...)
 * Usage: static_assert(UnsignedByte<T>);
 * @tparam T The type to be checked
 */
template <class T> concept UnsignedByte =
    std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == 1; // TODO: enum-friendly variant?

/**
 * @brief Concept to ensure a message format has a static ID and that `id` is
 * the first member.
 *
 * Example of a conforming type:
 * struct MessageFormat {
 *     static constexpr std::uint8_t ID = 0x01; // static ID
 *     std::uint8_t id; // must be first member
 *     std::uint32_t data; // other members
 *     std::array<std::uint8_t, 10> payload; // etc.
 * };
 *
 * Requirements:
 *  - `T::ID` is a constant expression, representable in std::uint8_t.
 *  - The type of `T::ID` is a 1-byte unsigned integral (or use the enum-friendly variant below).
 *  - `T` has a non-static data member `id` that is an unsigned 1-byte integral.
 *  - `id` is an lvalue (rules out bit-fields/proxies).
 *  - `T` is standard-layout and `offsetof(T, id) == 0`.
 *  - (Optional) `T` is trivially copyable for safe memcpy of the whole struct.
 *
 * @tparam T The message format type to be checked.
 */
template <typename T> concept MessageFormatT =
    requires { // must expose a `static constexpr std::uint8_t ID`
        requires UnsignedByte<decltype(T::ID)>;
        std::integral_constant<std::uint8_t, T::ID>{};
    } &&
    requires(T& x) { // must have a non-static member `std::uint8_t id`
        requires UnsignedByte<std::remove_cvref_t<decltype(x.id)>>;
        requires std::is_lvalue_reference_v<decltype((x.id))>;
    } && std::is_standard_layout_v<T> && // layout precondition for using offsetof
    offsetof(T, id) == 0;                // id must be the very first member

/* ―――――――――――――――― Classes ―――――――――――――――― */

/**
 * @brief Abstract base class representing a generic message
 *
 * This class provides functionality to serialize and deserialize messages.
 * It serves as a base for both received and sent messages.
 *
 * @tparam MessageFormat The format of the message content
 */
template <MessageFormatT MessageFormat> class Message
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
        if (content.size() != sizeof(MessageFormat)) { // TODO: handle structs with flexible array member?
            throw std::runtime_error("Invalid content size");
        }
        std::memcpy(&_content, content.data(), sizeof(MessageFormat));
    }

  public:
    /** @brief Type alias for the message format */
    using message_format_t = MessageFormat;

    virtual ~Message() = default;

    /** @brief Serializes the message content into a byte vector
     *
     * @return std::vector<std::uint8_t> Serialized byte vector of the message
     *content
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
 * This class inherits from Message and is used to represent messages that are
 * received.
 *
 * @tparam ReceivedMessageFormat The format of the received message
 */
template <MessageFormatT ReceivedMessageFormat> class ReceivedMessage : public Message<ReceivedMessageFormat>
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
 * This class inherits from Message and is used to represent messages that are
 * sent.
 *
 * @tparam SentMessageFormat The format of the sent message
 */
template <MessageFormatT SentMessageFormat> class SentMessage final : public Message<SentMessageFormat>
{
  public:
    /** @brief Constructs a SentMessage from structured content
     *
     * @param content Structured content of the sent message
     **/
    explicit SentMessage(SentMessageFormat content) : Message<SentMessageFormat>(std::move(content)) {}
};
