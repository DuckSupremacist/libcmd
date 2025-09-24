#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

/* ==== */
/* CORE */
/* ==== */

template <typename MessageFormat>
class Message : public MessageFormat
{
    MessageFormat _content;

protected:
    explicit Message(MessageFormat content) : _content(std::move(content))
    {
    }

    explicit Message(const std::vector<std::uint8_t>& content)
    {
        if (content.size() != sizeof(MessageFormat))
        {
            throw std::runtime_error("Invalid content size");
        }
        std::memcpy(&_content, content.data(), sizeof(MessageFormat));
    }

public:
    virtual ~Message() = default;

    [[nodiscard]] virtual std::vector<std::uint8_t> serialize() const
    {
        return {
            reinterpret_cast<const std::uint8_t*>(&_content),
            reinterpret_cast<const std::uint8_t*>(&_content) + sizeof(MessageFormat)
        };
    }

    [[nodiscard]] const MessageFormat& content() const
    {
        return _content;
    }
};

template <typename ReceivedMessageFormat>
class ReceivedMessage : public Message<ReceivedMessageFormat>
{
public:
    explicit ReceivedMessage(const std::vector<std::uint8_t>& content) : Message<ReceivedMessageFormat>(std::move(content))
    {
    }
};

template <typename SentMessageFormat>
class SentMessage : public Message<SentMessageFormat>
{
public:
    explicit SentMessage(SentMessageFormat content) : Message<SentMessageFormat>(std::move(content))
    {
    }
};

template<typename CommandMessageFormat, typename ResponseMessageFormat>
class Command : public ReceivedMessage<CommandMessageFormat>
{
public:
    explicit Command(const std::vector<std::uint8_t>& content) : ReceivedMessage<CommandMessageFormat>(std::move(content))
    {
    }

    [[nodiscard]] virtual SentMessage<ResponseMessageFormat> execute() const = 0;
};


/* ================= */
/* USER-SPECIFIC MSG */
/* ================= */

// Received message
struct SpecificReceivedMessageFormat
{
    std::uint8_t cmd_id;
    std::uint8_t arg;
} __attribute__((packed));

struct SpecificReceivedMessage final : ReceivedMessage<SpecificReceivedMessageFormat>
{
    explicit SpecificReceivedMessage(const std::vector<std::uint8_t>& content) : ReceivedMessage(content)
    {
    }
};

// Sent message
struct SpecificSentMessageFormat
{
    std::uint8_t cmd_id;
    std::uint8_t status;
    std::uint32_t value;
} __attribute__((packed));

struct SpecificSentMessage final : SentMessage<SpecificSentMessageFormat>
{
    explicit SpecificSentMessage(const SpecificSentMessageFormat& content) : SentMessage(content)
    {
    }
};

struct SpecificCommand final : Command<SpecificReceivedMessageFormat, SpecificSentMessageFormat>
{
    explicit SpecificCommand(const std::vector<std::uint8_t>& content) : Command(content)
    {
    }

    [[nodiscard]] SentMessage<SpecificSentMessageFormat> execute() const override
    {
        // Dummy implementation
        return SentMessage<SpecificSentMessageFormat>({
            .cmd_id = this->content().cmd_id,
            .status = 0x00,
            .value = 0x12345678,
        });
    }
};

/* ===== */
/* USAGE */
/* ===== */
static void printVector(const std::vector<std::uint8_t>& vec)
{
    for (const auto& byte : vec)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

int main()
{
    // reception
    const std::vector<std::uint8_t> received_data = {0x01, 0x05};
    const SpecificReceivedMessage recv_msg({0x01, 0x05});

    std::cout << "Received Message:" << std::endl
        << " - cmd_id: " << static_cast<int>(recv_msg.content().cmd_id) << std::endl
        << " - arg: " << static_cast<int>(recv_msg.content().arg) << std::endl;
    std::cout << "Serialized Received Message:\n 0x";
    printVector(recv_msg.serialize());


    // sending
    constexpr SpecificSentMessageFormat content = {
        .cmd_id = 0x01,
        .status = 0x02,
        .value = 0xDEADBEEF,
    };
    const SpecificSentMessage sent_msg(content);

    std::cout << "Sent Message:" << std::endl
        << " - cmd_id: " << static_cast<int>(sent_msg.content().cmd_id) << std::endl
        << " - status: " << static_cast<int>(sent_msg.content().status) << std::endl
        << " - value: " << sent_msg.content().value << std::endl;
    std::cout << "Serialized Sent Message:\n 0x";
    printVector(sent_msg.serialize());

    // command
    const SpecificCommand cmd(received_data);
    std::cout << "Command Message:" << std::endl
        << " - cmd_id: " << static_cast<int>(cmd.content().cmd_id) << std::endl
        << " - arg: " << static_cast<int>(cmd.content().arg) << std::endl;
    std::cout << "Serialized Command Message:\n 0x";
    printVector(cmd.serialize());

    const auto response = cmd.execute();
    std::cout << "Command Response Message:" << std::endl
        << " - cmd_id: " << static_cast<int>(response.content().cmd_id) << std::endl
        << " - status: " << static_cast<int>(response.content().status) << std::endl
        << " - value: " << response.content().value << std::endl;
    std::cout << "Serialized Command Response Message:\n 0x";
    printVector(response.serialize());

    return 0;
}
