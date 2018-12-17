#pragma once

#include <ozo/connection.h>
#include <ozo/time_traits.h>

namespace ozo {

template <typename Conn, typename Executor, typename CompletionToken>
inline auto cancel(Conn&& conn, Executor&& ex, const time_traits::duration& timeout, CompletionToken&& token) {
    static_assert(Connection<Conn>, "Conn must be Connection");

    using signature_t = void (error_code, std::decay_t<Conn>);
    async_completion<CompletionToken, signature_t> init(token);

    impl::async_cancel(std::forward<Conn>(conn), std::forward<Executor>(ex), timeout, init.completion_handler);

    return init.result.get();
}

template <typename Conn, typename CompletionToken>
inline auto cancel(Conn&& conn, const time_traits::duration& timeout, CompletionToken&& token) {
    return cancel(
        std::forward<Conn>(conn),
        asio::system_executor{},
        timeout,
        std::forward<CompletionToken>(token)
    );
}

template <typename Conn, typename CompletionToken>
inline auto cancel(Conn&& conn, CompletionToken&& token) {
    static_assert(Connection<Conn>, "Conn must be Connection");

    using signature_t = void (error_code, std::decay_t<Conn>);
    async_completion<CompletionToken, signature_t> init(token);
    impl::async_cancel(
        std::forward<Conn>(conn),
        asio::system_executor{},
        init.completion_handler
    );
    return init.result.get();
}

} // namespace ozo
