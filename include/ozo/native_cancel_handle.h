#pragma once

#include <libpq-fe.h>
#include <memory>
namespace std {

/**
 * @brief Default deleter for PGcancel
 *
 * There are two ways to specify deleter for std::unique_ptr
 * 1. By template parameter
 * 2. By "std::default_delete" template specialization
 *
 * In the first case we lose the default constructor for the pointer.
 * That's why the second way was chosen.
 */
template <>
struct default_delete<PGcancel> {
    void operator() (PGcancel *ptr) const { PQfreeCancel(ptr); }
};

} // namespace std

namespace ozo {

/**
 * libpq PGcancel safe RAII representation.
 */
using native_cancel_handle = std::unique_ptr<PGcancel>;

} // namespace ozo