#pragma once

#include <ozo/impl/io.h>
#include <ozo/connection.h>
#include <ozo/type_traits.h>

namespace ozo::impl {

template <typename Handler>
struct cancel_timeout_handler {
    Handler handler_;

    cancel_timeout_handler(Handler handler) : handler_(std::move(handler_)) {}

    void operator() (error_code ec) {
        if (ec != asio::error::operation_aborted) {
            handler(asio::error::operation_aborted);
        }
    }

    using executor_type = decltype(asio::get_associated_executor(handler_));

    executor_type get_executor() const noexcept { return asio::get_associated_executor(handler_);}

    using allocator_type = decltype(asio::get_associated_allocator(handler_));

    allocator_type get_allocator() const noexcept { return asio::get_associated_allocator(handler_);}
};

template <typename Connection, typename Handler>
struct cancel_op_handler {

    struct context_type {
        Connection conn;
        std::optional<Handler> handler;
    };

    std::shared_ptr<context_type> ctx_;

    cancel_op_handler(Connection conn, Handler handler) {
        auto allocator = asio::get_associated_allocator(handler);
        ctx_ = std::allocate_shared<context_type>(allocator, std::move(conn), std::move(handler));
    }

    void operator() (error_code ec) {
        if (ctx_->handler) {
            auto handler = std::move(*ctx_->handler);
            ctx_->handler = std::nullopt;
            std::move(handler)(std::move(ec), std::move(ctx_->conn));
        }
    }

    using executor_type = decltype(asio::get_associated_executor(ctx_->handler));

    executor_type get_executor() const noexcept { return asio::get_associated_executor(ctx_->handler);}

    using allocator_type = decltype(asio::get_associated_allocator(ctx_->handler));

    allocator_type get_allocator() const noexcept { return asio::get_associated_allocator(ctx_->handler);}
};

template <typename Executor, typename Connection, typename Handler>
struct cancel_op {
    Executor ex_;
    cancel_op_handler<Connection, Handler> handler_;

    cancel_op(const Executor& ex, cancel_op_handler<Connection, Handler> handler)
    : ex_(ex), handler_(std::move(handler)){}

    void perform() {
        asio::post(std::move(*this));
    }

    using executor_type = Executor;

    executor_type get_executor() const { return ex_;}

    void operator()() {
        auto h = get_cancel_handle(handler_.ctx_->conn);
        auto [ec, msg] = dispatch_cancel(h);
        if (ec) {
            set_error_context(handler_.ctx_->conn, std::move(msg));
        }
        asio::dispatch(bind(std::move(handler_), std::move(ec)));
    }
};

template <typename Conn, typename Executor, typename Handler>
inline void async_cancel(Conn&& conn, Executor&& ex, Handler&& h) {
    static_assert(Connection<Conn>, "Conn must be Connection");
    cancel_op op{ex, cancel_op_handler(std::forward<Conn>(conn), std::move(h))};
    op.perform();
}

template <typename Conn, typename Executor, typename Handler>
inline void async_cancel(Conn&& conn, Executor&& ex, const time_traits::duration& timeout, Handler&& h) {
    static_assert(Connection<Conn>, "Conn must be Connection");

    auto strand = ozo::detail::make_strand_executor(ozo::get_executor(conn));
    auto handler = cancel_op_handler {
        std::forward<Conn>(conn),
        asio::bind_executor(strand,
            detail::cancel_timer_handler(detail::post_handler(std::move(h)))
        )
    };
    get_timer(op.handler.ctx_->conn).expires_after(timeout);
    get_timer(op.handler.ctx_->conn).async_wait(cancel_timeout_handler{handler});

    async_cancel(std::forward<Conn>(conn), std::forward<Executor>(ex), std::move(handler));
}

} // namespace ozo::impl
