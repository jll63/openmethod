#ifndef BOOST_OPENMETHOD_DETAIL_TRACE_HPP
#define BOOST_OPENMETHOD_DETAIL_TRACE_HPP

#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace boost::openmethod::detail {

template<class Compiler>
struct trace_stream {
    bool on = false;
    std::size_t indentation_level{0};

    auto operator++() -> trace_stream& {
        if constexpr (Compiler::has_trace) {
            if (on) {
                for (std::size_t i = 0; i < indentation_level; ++i) {
                    Compiler::Registry::output::os << "  ";
                }
            }
        }

        return *this;
    }

    struct indent {
        trace_stream& trace;
        int by;

        explicit indent(trace_stream& trace, int by = 2)
            : trace(trace), by(by) {
            trace.indentation_level += by;
        }

        ~indent() {
            trace.indentation_level -= by;
        }
    };
};

struct rflush {
    std::size_t width;
    std::size_t value;
    explicit rflush(std::size_t width, std::size_t value)
        : width(width), value(value) {
    }
};

struct type_name {
    type_name(type_id type) : type(type) {
    }
    type_id type;
};

struct binary {
    std::uintptr_t value;

    binary(std::uintptr_t value) : value(value) {
    }
};

template<class Compiler>
auto operator<<(trace_stream<Compiler>& os, const binary& value)
    -> trace_stream<Compiler>& {
    if constexpr (Compiler::has_trace) {
        for (std::size_t i = sizeof(std::uintptr_t) * 8; i-- > 0;) {
            os << ((value.value & (std::uintptr_t(1) << i)) ? "1" : "0");
        }
    }

    return os;
}

} // namespace boost::openmethod::detail

#endif // BOOST_OPENMETHOD_DETAIL_TRACE_HPP
