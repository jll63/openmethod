// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_POLICY_MINIMAL_COVER_HASH_HPP
#define BOOST_OPENMETHOD_POLICY_MINIMAL_COVER_HASH_HPP

#include <boost/openmethod/preamble.hpp>
#include <boost/openmethod/detail/trace.hpp>

#include <boost/config.hpp>

#include <limits>
#include <variant>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable code
#endif

namespace boost::openmethod {

namespace detail {

template<class Registry>
std::vector<type_id> minimal_cover_hash_control;

} // namespace detail

namespace policies {

//! Hash type ids using a minimal cover hash function.
//!
//! `minimal_cover_hash` implements the @ref type_hash policy using a
//! hash function that attempts to find a minimal covering set for the
//! registered type_ids.
//!
//! @note This is a skeleton implementation. The actual hashing algorithm
//! needs to be implemented.
struct minimal_cover_hash : type_hash {

    //! Hash construction error
    struct construction_error : openmethod_error {
        //! Number of types that could not be hashed
        std::size_t failed_types;

        //! Write a short description to an output stream
        //! @param os The output stream
        //! @tparam Registry The registry
        //! @tparam Stream A @ref LightweightOutputStream
        template<class Registry, class Stream>
        auto write(Stream& os) const -> void;
    };

    using errors = std::variant<construction_error>;

    //! A TypeHashFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy
    template<class Registry>
    class fn {
        static std::size_t min_value;
        static std::size_t max_value;

        static void check(std::size_t index, type_id type);

        template<class InitializeContext, class... Options>
        static void initialize(
            const InitializeContext& ctx, std::vector<type_id>& control,
            const std::tuple<Options...>& options);

      public:
        //! Initialize the hash function
        //!
        //! Constructs the minimal cover hash for the specified input values.
        //!
        //! If construction fails, calls the error handler with
        //! a @ref construction_error object then calls `abort`.
        //!
        //! @tparam Context An @ref InitializeContext.
        //! @param ctx A Context object.
        //! @param options Initialization options.
        //! @return A pair containing the minimum and maximum hash values.
        template<class Context, class... Options>
        static auto
        initialize(const Context& ctx, const std::tuple<Options...>& options) {
            if constexpr (Registry::has_runtime_checks) {
                initialize(
                    ctx, detail::minimal_cover_hash_control<Registry>, options);
            } else {
                std::vector<type_id> control;
                initialize(ctx, control, options);
            }

            return std::pair{min_value, max_value};
        }

        //! Hash a type id
        //!
        //! Hash a type id using the minimal cover hash function.
        //!
        //! If `Registry` contains the @ref runtime_checks policy, checks that
        //! the type id is valid, i.e. if it was present in the set passed to
        //! @ref initialize. Its absence indicates that a class involved in a
        //! method definition, method overrider, or method call was not
        //! registered. In this case, signal a @ref missing_class using
        //! the registry's @ref error_handler if present; then calls `abort`.
        //!
        //! @param type The type_id to hash
        //! @return The hash value
        BOOST_FORCEINLINE
        static auto hash(type_id type) -> std::size_t {
            // TODO: Implement the actual hash function
            auto index = reinterpret_cast<detail::uintptr>(type);

            if constexpr (Registry::has_runtime_checks) {
                check(index, type);
            }

            return index;
        }

        //! Releases the memory allocated by `initialize`.
        //!
        //! @tparam Options... Zero or more option types, deduced from the function
        //! arguments.
        //! @param options Zero or more option objects.
        template<class... Options>
        static auto finalize(const std::tuple<Options...>&) -> void {
            detail::minimal_cover_hash_control<Registry>.clear();
        }
    };
};

template<class Registry>
std::size_t minimal_cover_hash::fn<Registry>::min_value;

template<class Registry>
std::size_t minimal_cover_hash::fn<Registry>::max_value;

template<class Registry>
template<class InitializeContext, class... Options>
void minimal_cover_hash::fn<Registry>::initialize(
    const InitializeContext& ctx, std::vector<type_id>& control,
    const std::tuple<Options...>& options) {
    (void)options;

    if constexpr (InitializeContext::template has_option<trace>) {
        ctx.tr << "Initializing minimal_cover_hash\n";
    }

    auto all_zero_mask = std::uintptr_t();
    auto all_one_mask = ~std::uintptr_t();

    for (const auto& cls : ctx.classes) {
        for (auto id : cls.type_ids) {
            all_zero_mask |= reinterpret_cast<std::uintptr_t>(id);
            all_one_mask &= reinterpret_cast<std::uintptr_t>(id);
        }
    }

    if constexpr (InitializeContext::has_trace) {
        ctx.tr << "all zero mask: " << detail::binary(all_zero_mask) << "\n";
        ctx.tr << "all one  mask: " << detail::binary(all_one_mask) << "\n";
    }
}

template<class Registry>
void minimal_cover_hash::fn<Registry>::check(std::size_t index, type_id type) {
    if (index < min_value || index > max_value ||
        detail::minimal_cover_hash_control<Registry>[index] != type) {

        if constexpr (Registry::has_error_handler) {
            missing_class error;
            error.type = type;
            Registry::error_handler::error(error);
        }

        abort();
    }
}

template<class Registry, class Stream>
auto minimal_cover_hash::construction_error::write(Stream& os) const -> void {
    os << "minimal_cover_hash construction failed for " << failed_types
       << " types\n";
}

} // namespace policies
} // namespace boost::openmethod

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
