#include <ozo/connection_info.h>
#include <ozo/connection_pool.h>
#include <ozo/request.h>
#include <ozo/shortcuts.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>

#include <iostream>

namespace asio = boost::asio;

int main(int argc, char **argv) {
    std::cout << "OZO connection pool example" << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <connection string>\n";
        return 1;
    }

    // Ozo perform all IO using Boost.Asio, so the first thing we need to do is to setup asio::io_context
    asio::io_context io;

    //! [Creating Connection Pool]

    // To make a connection to a database we need to make a ConnectionSource.
    auto connection_info = ozo::make_connection_info(argv[1]);

    ozo::connection_pool_config connection_pool_config;

    // Maximum limit for number of stored connections
    connection_pool_config.capacity = 1;
    // Maximum limit for number of waiting requests for connection
    connection_pool_config.queue_capacity = 10;
    // Maximum time duration to store unused open connection
    connection_pool_config.idle_timeout = std::chrono::seconds(60);

    // Creating connection pool from connection_info as the underlying ConnectionSource
    auto connection_pool = ozo::make_connection_pool(connection_info, connection_pool_config);
    //! [Creating Connection Pool]

    const auto coroutine = [&] (asio::yield_context yield) {

        //! [Creating Connection Provider]

        // The next step is bind asio::io_context with ConnectionSource to setup executor for all
        // callbacks. Default connection is a ConnectionProvider. If there is some problem with network
        // or database we don't want to wait indefinitely, so we establish connect timeout. If there is
        // no available connection in the connection pool we also want to wait within finite time duration.
        ozo::connection_pool_timeouts timeouts;
        timeouts.connect = std::chrono::seconds(1);
        timeouts.queue = std::chrono::seconds(1);

        const auto connector = ozo::make_connector(connection_pool, io, timeouts);
        //! [Creating Connection Provider]

        // Request result is always set of rows. Client should take care of output object lifetime.
        ozo::rows_of<int> result;

        // Request operation require ConnectionProvider, query, output object for result and CompletionToken.
        // Also we setup request timeout and reference for error code to avoid throwing exceptions.
        // Function returns connection which can be used as ConnectionProvider for futher requests or to
        // get additional inforation about error through error context.
        const std::chrono::seconds request_timeout(1);
        boost::system::error_code ec;
        // This allows to use _SQL literals
        using namespace ozo::literals;
        const auto connection = ozo::request(
            connector,
            "SELECT pg_backend_pid()"_SQL,
            request_timeout,
            ozo::into(result),
            yield[ec]
        );

        // When request is completed we check is there an error. This example should not produce any errors
        // if there are no problems with target database, network or permissions for given user in connection
        // string.
        if (ec) {
            std::cout << "Request failed with error: " << ec.message()
                << ", error context: " << ozo::get_error_context(connection) << std::endl;
            return;
        }

        // Just print request result
        std::cout << "Selected:" << std::endl;
        for (auto value : result) {
            std::cout << std::get<0>(value) << std::endl;
        }
    };

    // All IO is asynchronous, therefore we have a choice here, what should be our CompletionToken.
    // We use Boost.Coroutines to write asynchronouse code in synchronouse style. Run two coroutines
    // to demonstrate concurrent request from pool and connection reusage.
    for (int i = 0; i < 2; ++i)
        asio::spawn(io, coroutine);

    io.run();

    return 0;
}
