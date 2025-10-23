#pragma once

#include "handler.h"
#include "result.h"

/* ―――――――――――――――― Concepts ―――――――――――――――― */

template <typename H> concept HandlerLike =
    requires(const std::vector<std::uint8_t>& data, const Communicator& communicator) {
        // Ensure static ID exists and is convertible to uint8_t
        { H::ID } -> std::convertible_to<std::uint8_t>;
        // Ensure execute signature exists
        { H::execute(data, communicator) };
    };

/* ―――――――――――――――― Helpers ―――――――――――――――― */

namespace meta_handler_helpers
{

template <HandlerLike H> consteval std::uint8_t handlerId() { return H::ID; }

template <HandlerLike...> struct UniqueIds : std::true_type
{};

template <HandlerLike H, HandlerLike... Rest> struct UniqueIds<H, Rest...>
    : std::bool_constant<((handlerId<H>() != handlerId<Rest>()) && ...) && UniqueIds<Rest...>::value>
{};

} // namespace meta_handler_helpers

/* ―――――――――――――――― Classes ―――――――――――――――― */

using MetaHandlerExecuteResult = Result<void, std::string>;

/**
 * @brief Class that handles execution of Handlers based on incoming data
 *
 * This class takes a variadic list of Handler-like types and provides a static
 * method to execute the appropriate Handler based on the ID found in the
 * incoming data. It ensures at compile-time that all Handler IDs are unique.
 *
 * @tparam Handlers Variadic list of Handler-like types
 */
template <HandlerLike... Handlers> class MetaHandler final
{
    static_assert(
        meta_handler_helpers::UniqueIds<Handlers...>::value, "Duplicate Handler IDs registered in MetaHandler"
    );

  public:
    /**
     * @brief Executes the appropriate Handler based on incoming data
     * @param port The port number associated with the incoming data, serve Handler selection
     * @param data Raw byte data containing the Handler ID and payload
     * @param communicator The Communicator instance to handle responses and requests
     * @return MetaHandlerExecuteResult The result of the Handler execution
     */
    [[nodiscard]] static MetaHandlerExecuteResult
    execute(const std::uint8_t port, const std::vector<std::uint8_t>& data, const Communicator& communicator) noexcept {
        // Short-circuit fold: constructs and execute only the matching Handler
        HandlerExecuteResult out;
        const bool matched = ((port == Handlers::ID && (out = Handlers::execute(data, communicator), true)) || ...);
        if (!matched) {
            return unexpected("Unknown Handler ID: " + std::to_string(port));
        }
        if (!out) {
            return unexpected(out.error());
        }
        return {};
    }
};
