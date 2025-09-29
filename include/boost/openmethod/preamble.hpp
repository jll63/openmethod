#ifndef BOOST_OPENMETHOD_REGISTRY_HPP
#define BOOST_OPENMETHOD_REGISTRY_HPP

#include <boost/openmethod/detail/static_list.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>

#include <stdlib.h>
#include <vector>
#include <cstdint>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace boost::openmethod {

namespace detail {

union word {
    word() {
    } // undefined
    word(void (*pf)()) : pf(pf) {
    }
    word(word* pw) : pw(pw) {
    }
    word(std::size_t i) : i(i) {
    }

    void (*pf)();
    std::size_t i;
    word* pw;
};

} // namespace detail

//! Alias to v-table pointer type.
//!
//! `vptr_type` is an alias to the type of a v-table pointer.
using vptr_type = const detail::word*;

//! Type used to identify a class.
//!
//! `type_id` is the return type of the @ref static_type and @ref dynamic_type
//! functions. It can be used as an actual data pointer (e.g. to a
//! `std::type_info` object), or as an opaque integer type.
using type_id = const void*;

//! Decorator for virtual parameters.
//!
//! `virtual_` marks a formal parameter of a method as virtual. It is a @em
//! decorator, not an actual type that can be instantiated (it does not have a
//! definition). It is removed from the method's signature.
//!
//! @note `virtual_` can be used @em only in method declarations, @em not in
//! overriders. A parameter in overriders is implicitly virtual if it is in
//! the same position as a virtual parameter in the method's declaration.
//!
//! @par Requirements
//!
//! - @ref virtual_traits must be specialized for `T`.
//!
//! @tparam T A class.
template<typename T>
struct virtual_;

template<typename T, class Registry>
struct virtual_traits;

// -----------------------------------------------------------------------------
// Error handling

//! Base class for all OpenMethod errors.
struct openmethod_error {};

struct odr_violation : openmethod_error {
    //! Write a description of the error to a stream.
    //! @tparam Registry The registry containing this policy.
    //! @param stream The stream to write to.
    template<class Registry, class Stream>
    auto write(Stream& stream) const -> void {
        stream << "conflicting definitions of ";
        Registry::rtti::type_name(
            Registry::rtti::template static_type<Registry>(), stream);
    }
};

template<class Registry>
struct odr_check {
    inline static std::size_t count;
    template<class R>
    inline static std::size_t inc = count++;

    odr_check() {
        [[maybe_unused]] auto _ = &inc<typename Registry::registry_type>;
    }
};

//! Registry not initialized
struct not_initialized : openmethod_error {
    //! Write a short description to an output stream
    //! @param os The output stream
    //! @tparam Registry The registry
    //! @tparam Stream A @ref LightweightOutputStream
    template<class Registry, class Stream>
    auto write(Stream& os) const -> void {
        os << "not initialized";
    }
};

//! Missing class.
//!
//! A class used as a virtual parameter in a method, an overrider or a method
//! call was not registered.
//!
//! @par Examples
//!
//! Missing registration of a class used as a virtual parameter in a method:
//! @code
//! struct Animal { virtual ~Animal() {} };
//! struct Dog : Animal {};
//!
//! BOOST_OPENMETHOD_CLASSES(Animal);
//!
//! BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), void);
//!
//! initialize(); // throws missing_class;
//! @endcode
//!
//! Missing registration of a class used as a virtual parameter in an overrider:
//! @code
//! BOOST_OPENMETHOD_CLASSES(Animal);
//!
//! BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), void);
//!
//! BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog>), void) { /* ... */ }
//!
//! initialize(); // throws missing_class;
//! @endcode
//!
//! Missing registration of a class used as a virtual parameter in a call:
//! @code
//! struct Bulldog : Dog {};
//!
//! BOOST_OPENMETHOD_CLASSES(Animal, Dog);
//!
//! BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), void);
//!
//! BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog>), void) { /* ... */ }
//!
//! Bulldog hector;
//! poke(hector); // throws missing_class;
//! @endcode
struct missing_class : openmethod_error {
    //! The type_id of the unknown class.
    type_id type;

    //! Write a short description to an output stream
    //! @param os The output stream
    //! @tparam Registry The registry
    //! @tparam Stream A @ref LightweightOutputStream
    template<class Registry, class Stream>
    auto write(Stream& os) const -> void;
};

//! Missing base.
//!
//! A class used in an overrider virtual parameter was not registered as a
//! derived class of the class in the same position in the method's virtual
//! parameter list.
//!
//! @par Example
//! In the following code, OpenMethod cannot infer that `Dog` is derived from
//! `Animal`, because they are not registered in a same call to @ref
//! BOOST_OPENMETHOD_CLASSES.
//!
//! @code
//! BOOST_OPENMETHOD_CLASSES(Animal);
//! BOOST_OPENMETHOD_CLASSES(Dog);
//!
//! BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), void);
//!
//! BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog>), void) { /* ... */ }
//!
//! initialize(); // throws missing_base;
//! @endcode
//!
//! Fix:
//!
//! @code
//! BOOST_OPENMETHOD_CLASSES(Animal, Dog);
//! @endcode
struct missing_base : openmethod_error {
    //! The type_id of the base class.
    type_id base;
    //! The type_id of the derived class.
    type_id derived;

    //! Write a short description to an output stream
    //! @param os The output stream
    //! @tparam Registry The registry
    //! @tparam Stream A @ref LightweightOutputStream
    template<class Registry, class Stream>
    auto write(Stream& os) const -> void;
};

//! No valid overrider
struct bad_call : openmethod_error {
    //! The type_id of method that was called
    type_id method;
    //! The number of @em virtual arguments in the call
    std::size_t arity;
    //! The maximum size of `types`
    static constexpr std::size_t max_types = 16;
    //! The type_ids of the arguments.
    type_id types[max_types];

  protected:
    template<class Registry, class Stream>
    auto write_aux(Stream& os, const char* subtype) const -> void;
};

//! No overrider for virtual tuple
//!
//! @see @ref bad_call for data members.
struct no_overrider : bad_call {
    //! Write a short description to an output stream
    //! @param os The output stream
    //! @tparam Registry The registry
    //! @tparam Stream A @ref LightweightOutputStream
    template<class Registry, class Stream>
    auto write(Stream& os) const -> void {
        write_aux<Registry>(os, "not implemented");
    }
};

//! Ambiguous call
//!
//! @see @ref bad_call for data members.
struct ambiguous_call : bad_call {
    //! Write a short description to an output stream
    //! @param os The output stream
    //! @tparam Registry The registry
    //! @tparam Stream A @ref LightweightOutputStream
    template<class Registry, class Stream>
    auto write(Stream& os) const -> void {
        write_aux<Registry>(os, "ambiguous");
    }
};

//! Static and dynamic type mismatch in "final" construct
//!
//! If runtime checks are enabled, the "final" construct checks that the static
//! and dynamic types of the object, as reported by the `rtti` policy,  are the
//! same. If they are not, and if the registry contains an @ref error_handler
//! policy, its @ref error function is called with a `final_error` object, then
//! the program is terminated with
//! @ref abort.
struct final_error : openmethod_error {
    type_id static_type, dynamic_type;

    //! Write a short description to an output stream
    //! @param os The output stream
    //! @tparam Registry The registry
    //! @tparam Stream A @ref LightweightOutputStream
    template<class Registry, class Stream>
    auto write(Stream& os) const -> void;
};

namespace detail {

struct empty {};

template<typename Iterator>
struct range {
    range(Iterator first, Iterator last) : first(first), last(last) {
    }

    Iterator first, last;

    auto begin() const -> Iterator {
        return first;
    }

    auto end() const -> Iterator {
        return last;
    }
};

// -----------------------------------------------------------------------------
// class info

struct class_info : static_list<class_info>::static_link {
    type_id type;
    vptr_type* static_vptr;
    type_id *first_base, *last_base;
    bool is_abstract{false};

    auto vptr() const {
        return static_vptr;
    }

    auto type_id_begin() const {
        return &type;
    }

    auto type_id_end() const {
        return &type + 1;
    }
};

struct deferred_class_info : class_info {
    virtual void resolve_type_ids() = 0;
};

// -----------
// method info

struct overrider_info;

struct method_info : static_list<method_info>::static_link {
    type_id* vp_begin;
    type_id* vp_end;
    static_list<overrider_info> specs;
    void (*not_implemented)();
    void (*ambiguous)();
    type_id method_type_id;
    type_id return_type_id;
    std::size_t* slots_strides_ptr;

    auto arity() const {
        return std::distance(vp_begin, vp_end);
    }
};

struct deferred_method_info : method_info {
    virtual void resolve_type_ids() = 0;
};

struct overrider_info : static_list<overrider_info>::static_link {
    ~overrider_info() {
        method->specs.remove(*this);
    }

    method_info* method; // for the destructor, to remove definition
    type_id return_type; // for N2216 disambiguation
    type_id type;        // of the function, for trace
    void (**next)();
    type_id *vp_begin, *vp_end;
    void (*pf)();
};

struct deferred_overrider_info : overrider_info {
    virtual void resolve_type_ids() = 0;
};

} // namespace detail

#ifdef __MRDOCS__

//! Blueprint for a lightweight output stream (exposition only).
//!
//! Classes used as output streams in policies must provide the free functions
//! described on this page.
struct LightweightOutputStream {
    LightweightOutputStream& operator<<(const char* str);
    LightweightOutputStream& operator<<(const std::string_view& view);
    LightweightOutputStream& operator<<(const void* value);
    LightweightOutputStream& operator<<(std::size_t value);
};

#endif

namespace detail {

struct option_base {};

} // namespace detail

//! N2216 ambiguity resolution.
//!
//! If `n2216` is present in @ref initialize\'s `Options`, additional steps are
//! taken to select a single overrider in presence of ambiguous overriders sets,
//! according to the rules defined in the N2216 paper. If the normal resolution
//! procedure fails to select a single overrider, the following steps are
//! applied, in order:
//!
//! - If the return types of the remaining overriders are all polymorphic and
//!   covariant, and one of the return types is more specialized thjat all the
//!   others, use it.
//!
//! - Otherwise, pick one of the overriders. Which one is used is unspecified,
//!   but remains the same throughtout the program, and across different runs of
//!   the same program.
struct n2216 : detail::option_base {};

//! Enable `initialize` tracing.
//!
//! If `trace` is present in @ref initialize\'s `Options`, tracing code is added
//! to various parts of the initialization process (dispatch table construction,
//! hash factors search, etc). The tracing code is executed only if
//! @ref trace::on is set to `true`. The default value of `on` is `true` if
//! environment variable `BOOST_OPENMETHOD_TRACE` is set to the string "1", and
//! false otherwise.
//!
//! `trace` requires the registry being initialized to have an @ref output
//! policy.
//!
//! The content of the trace is neither specified, nor stable across versions.
//! It is comprehensive, and useful for troubleshooting missing class
//! registrations, missing or ambiguous overriders, etc.
struct trace : detail::option_base {
    //! Enable trace.
    static bool on;
};

//! Namespace for the policies.
//!
//! @see @ref registry for an explanation of policies.

namespace policies {

#ifdef __MRDOCS__

//! Type ids and v-table pointers for a class (exposition only).
struct IdsToVptr {
    //! Returns an iterator to the beginning of a range of `type_id`s for a
    //! single registered class.
    auto type_id_begin() const;

    //! Returns an iterator to the end of a range of `type_id`s for a
    //! single registered class.
    auto type_id_end() const;

    //! Returns a range of `type_id`s that this assignment applies to.
    auto vptr() const -> const vptr_type&;
};

#endif

#ifdef __MRDOCS__

//! Blueprint for `rtti` meta-functions (exposition only).
template<class Registry>
struct RttiFn {
    //! Tests if a class is polymorphic.
    //!
    //! @tparam Class A class.
    template<class Class>
    static constexpr bool is_polymorphic = std::is_polymorphic_v<Class>;

    //! Returns the static @ref type_id of a type.
    //!
    //! @note `Class` is not necessarily a @e registered class. This
    //! function is also called to acquire the type_id of non-virtual
    //! parameters, library types, etc, for diagnostic and trace purposes.
    //!
    //! @tparam Class A class.
    //! @return The static type_id of Class.
    template<class Class>
    static auto static_type() -> type_id;

    //! Returns the dynamic @ref type_id of an object.
    //!
    //! @tparam Class A registered class.
    //! @param obj A reference to an instance of `Class`.
    //! @return The type_id of `obj`'s class.
    template<class Class>
    static auto dynamic_type(const Class& obj) -> type_id;

    //! Writes a representation of a @ref type_id to a stream.
    //!
    //! @tparam Stream A LightweightOutputStream.
    //! @param type The `type_id` to write.
    //! @param stream The stream to write to.
    template<typename Stream>
    static auto type_name(type_id type, Stream& stream) -> void;

    //! Returns a key that uniquely identifies a class.
    //!
    //! @param type A `type_id`.
    //! @return A unique value that identifies a class with the given
    //! `type_id`.
    static auto type_index(type_id type);

    //! Casts an object to a type.
    //!
    //! @tparam D A reference to a subclass of `B`.
    //! @tparam B A registered class.
    //! @param obj A reference to an instance of `B`.
    template<typename D, typename B>
    static auto dynamic_cast_ref(B&& obj) -> D;
};

#endif

//! Policy for type information.
//!
//! `rtti` policies are responsible for type information acquisition and dynamic
//! casting.
//!
//! @par Requirements
//!
//! Classes implementing this policy must:
//! @li derive from `rtti`.
//! @li provide a `fn<Registry>` meta-function that conforms to the @ref RttiFn
//! blueprint.
struct rtti {
    // Policy category.
    using category = rtti;

    //! Default implementations of some `rtti` requirements.
    struct defaults {
        //! Default implementation for `type_index`.
        //!
        //! @param type A `type_id`.
        //!
        //! @return `type` itself.
        static auto type_index(type_id type) -> type_id {
            return type;
        }

        //! Default implementation of `type_name`.
        //!
        //! Executes `stream << "type_id(" << type << ")"`.
        //!
        //! @param type A `type_id`.
        //! @param stream A stream to write to.
        template<typename Stream>
        static void type_name(type_id type, Stream& stream) {
            stream << "type_id(" << type << ")";
        }
    };
};

#ifdef __MRDOCS__
struct std_rtti;
struct static_rtti;
#endif

//! Policy for deferred type id collection.
//!
//! Some custom RTTI systems rely on static constructors to assign type ids.
//! OpenMethod itself relies on static constructors to register classes, methods
//! and overriders. This creates order-of-initialization issues. Deriving a @e
//! rtti policy from this class - instead of just `rtti` - causes the collection
//! of type ids to be deferred until the first call to @ref update.
struct deferred_static_rtti : rtti {};

#ifdef __MRDOCS__
//! Blueprint for `error_handler` meta-functions (exposition only).
template<class Registry>
struct ErrorHandlerFn {
    //! Called when an error is detected.
    //!
    //! `error` is a function, or a set of functions, that can be called
    //! with an instance of any subclass of `openmethod_error`.
    static auto error(const auto& error) -> void;
};
#endif

//! Policy for error handling.
//!
//! A @e error_handler policy runs code before the library terminates the
//! program due to an error. This can be useful for throwing, logging, cleanup,
//! or other actions.
//!
//! @par Requirements
//!
//! Classes implementing this policy must:
//! @li derive from `error_handler`.
//! @li provide a `fn<Registry>` meta-function that conforms to the @ref
//! ErrorHandlerFn blueprint.
struct error_handler {
    // Policy category.
    using category = error_handler;
};

#ifdef __MRDOCS__

//! Blueprint for `vptr` meta-functions (exposition only).
//!
//! @tparam Registry The registry containing the policy.
template<class Registry>
struct VptrFn {
    //! Register the v-table pointers.
    //!
    //! Called by @ref registry::initialize to let the policy store the v-table
    //! pointer associated to each `type_id`.
    //!
    //! @tparam ForwardIterator An iterator to a range of @ref IdsToVptr
    //! objects.
    //! @param first The beginning of the range.
    //! @param last The end of the range.
    template<typename ForwardIterator>
    static auto initialize(ForwardIterator first, ForwardIterator last);

    //! Return a *reference* to a v-table pointer for an object.
    //!
    //! @tparam Class A registered class.
    //! @param arg A reference to a const object of type `Class`.
    //! @return A reference to a the v-table pointer for `Class`.
    template<class Class>
    static auto dynamic_vptr(const Class& arg) -> const vptr_type&;

    //! Release the resources allocated by `initialize`. This function is
    //! optional.
    static auto finalize() -> void;
};

#endif

//! Policy for v-table pointer acquisition.
//!
//! @par Requirements
//!
//! Classes implementing this policy must:
//! @li derive from `vptr`.
//! @li provide a `fn<Registry>` meta-function that conforms to the @ref
//! VptrFn blueprint.
struct vptr {
    // Policy category.
    using category = vptr;
};

//! Policy to add an indirection to pointers to v-tables.
//!
//! If this policy is present, constructs like @ref virtual_ptr, @ref
//! inplace_vptr, @ref vptr_vector, etc store pointers to pointers to v-tables.
//! These indirect pointers remain valid after a call to @ref initialize, even
//! though the v-tables move to different locations. This is useful in presence
//! of dynamic loading.
//!
//! @par Requirements
//!
//! None. `indirect_vptr` can be added to a registry's policy list as-is.

struct indirect_vptr final {
    // Policy category.
    using category = indirect_vptr;
    template<class Registry>
    struct fn {};
};

#ifdef __MRDOCS__
//! Blueprint for `type_hash` meta-functions (exposition only).
//!
//! @tparam Registry The registry containing the policy.
template<class Registry>
struct TypeHashFn {
    //! Initialize the hash table.
    //! @tparam ForwardIterator An iterator to a range of const
    //! @ref IdsToVptr objects.
    //! @param first The beginning of the range.
    //! @param last The end of the range.
    //! @return A pair containing the minimum and maximum hash values.
    template<typename ForwardIterator>
    static auto initialize(ForwardIterator first, ForwardIterator last)
        -> std::pair<std::size_t, std::size_t>;

    //! Hash a `type_id`.
    //! @param type A @ref type_id.
    //! @return A hash value for the given `type_id`.
    static auto hash(type_id type) -> std::size_t;

    //! Release the resources allocated by `initialize`. This function is
    //! optional.
    static auto finalize() -> void;
};
#endif

//! Policy hashing type ids.
//!
//! @par Requirements
//!
//! Classes implementing this policy must:
//! @li derive from `rtti`.
//! @li provide a `fn<Registry>` meta-function that conforms to the @ref
//! TypeHashFn blueprint.
struct type_hash {
    // Policy category.
    using category = type_hash;
};

#ifdef __MRDOCS__

//! Blueprint for @ref output meta-functions (exposition only).
//!
//! @tparam Registry The registry containing the policy.
template<class Registry>
struct OutputFn {
    //! A @ref LightweightOutputStream.
    inline static LightweightOutputStream os;
};

#endif

//! Policy for writing diagnostics and trace.
//!
//! If an `output` policy is present, the default error handler uses it to write
//! error messages to its output stream. @ref registry::initialize can also use
//! it to write trace messages.
//!
//! @par Requirements
//!
//! Classes implementing this policy must:
//! @li derive from `output`.
//! @li provide a `fn<Registry>` meta-function that conforms to the @ref
//! OutputFn blueprint.
struct output {
    // Policy category.
    using category = output;
};

//! Policy for post-initialize runtime checks.
//!
//! If this policy is present, performs the following checks:
//! @li Classes of virtual arguments have been registered.
//! @li Dynamic and static types match in "final" constructs (@ref
//! final_virtual_ptr and related functions).
struct runtime_checks final {
    // Policy category.
    using category = runtime_checks;
    template<class Registry>
    struct fn {};
};

} // namespace policies

namespace detail {

struct registry_base {};

template<typename T>
constexpr bool is_registry = std::is_base_of_v<registry_base, T>;

template<typename T>
constexpr bool is_not_void = !std::is_same_v<T, void>;

template<
    class Registry, class Index,
    class Size = mp11::mp_size<typename Registry::policy_list>>
struct get_policy_aux {
    using type = typename mp11::mp_at<
        typename Registry::policy_list, Index>::template fn<Registry>;
};

template<class Registry, class Size>
struct get_policy_aux<Registry, Size, Size> {
    using type = void;
};

using class_catalog = detail::static_list<detail::class_info>;
using method_catalog = detail::static_list<detail::method_info>;

template<class Policies, class...>
struct with_aux;

template<class Policies>
struct with_aux<Policies> {
    using type = Policies;
};

template<class Policies, class Policy, class... MorePolicies>
struct with_aux<Policies, Policy, MorePolicies...> {
    using replace = mp11::mp_replace_if_q<
        Policies,
        mp11::mp_bind_front_q<
            mp11::mp_quote_trait<std::is_base_of>, typename Policy::category>,
        Policy>;
    using replace_or_add = std::conditional_t<
        std::is_same_v<replace, Policies>, mp11::mp_push_back<Policies, Policy>,
        replace>;
    using type = typename with_aux<replace_or_add, MorePolicies...>::type;
};

template<class Policies, class...>
struct without_aux;

template<class Policies>
struct without_aux<Policies> {
    using type = Policies;
};

template<class Policies, class Policy, class... MorePolicies>
struct without_aux<Policies, Policy, MorePolicies...> {
    using type = typename without_aux<
        mp11::mp_remove_if_q<
            Policies,
            mp11::mp_bind_front_q<
                mp11::mp_quote_trait<std::is_base_of>,
                typename Policy::category>>,
        MorePolicies...>::type;
};

template<class...>
struct use_class_aux;

template<typename, class...>
struct initialize_aux;

} // namespace detail

//! Methods, classes and policies.
//!
//! Methods exist in the context of a registry. Any class used as a method or
//! overrider parameter, or in as a method call argument, must be registered
//! with the same registry.
//!
//! Before calling a method, its registry must be initialized with the @ref
//! initialize function. This is typically done at the beginning of `main`.
//!
//! Multiple registries can co-exist in the same program. They must be
//! initialized individually. Classes referenced by methods in different
//! registries must be registered with each registry.
//!
//! A registry also contains a set of @ref policies that control how certain
//! operations are performed. For example, the `rtti` policy provides type
//! information, implements dynamic casting, etc. It can be replaced to
//! interface with custom RTII systems (like LLVM's).
//!
//! Policies are implemented as Boost.MP11 quoted meta-functions. A policy class
//! must contain a `fn<Registry>` template that provides a set of static
//! members, specific to the responsibility of the policy. Registries
//! instantiate policies by passing themselves to the nested `fn` class
//! templates.
//!
//! There are two reason for this design.
//!
//! Some policies are "stateful": they contain static _data_ members. Since
//! several registries can co-exist in the same program, each stateful policy
//! needs its own, separate set of static data members. For example, @ref
//! vptr_vector, a "vptr" policy, contains a static vector of vptrs, which
//! cannot be shared with other registries.
//!
//! Also, some policies need access to other policies in the same registry. They
//! can be accessed via the `Registry` template parameter. For example, @ref
//! vptr_vector hashes type_ids before using them as an indexes, if `Registry`
//! cotains a `type_hash` policy. It performs out-of-bounds checks if `Registry`
//! contains the `runtime_checks` policy. If an error is detected, it invokes
//! the @ref error_handler policy if there is  one.
//!
//! @tparam Policy The policies used in the registry.
//!
//! @par Requirements
//!
//! @li `Policy` must contain a `category` alias to its root base class. The
//! registry may contain at most one policy per category.
//!
//! @li `Policy` must contain a `fn<Registry>` meta-function.
//!
//! @see @ref policies
template<class... Policy>
class registry : detail::registry_base {
    inline static detail::class_catalog classes;
    inline static detail::method_catalog methods;

    template<class...>
    friend struct detail::use_class_aux;
    template<typename Name, typename ReturnType, class Registry>
    friend class method;

    inline static std::vector<detail::word> dispatch_data;
    inline static bool initialized;

    template<class... Options>
    struct compiler;

  public:
    //! The type of this registry.
    using registry_type = registry;

    template<typename, class...>
    friend struct detail::initialize_aux;

    //! Check that the registry is initialized.
    //!
    //! Check if `initialize` has been called for this registry, and report an
    //! error if not.
    //!
    //! @par Errors
    //!
    //! @li @ref not_initialized: The registry is not initialized.
    static void require_initialized();

    //! Releases the resources held by the registry.
    //!
    //! `finalize` may be called to release any resources allocated by
    //! @ref registry::initialize.
    //!
    //! @note
    //! A translation unit that contains a call to `finalize` must include the
    //! `<boost/openmethod/initialize.hpp>` header.
    static void finalize();

    //! A pointer to the virtual table for a registered class.
    //!
    //! `static_vptr` is set by @ref registry::initialize to the address of the
    //! class' virtual table. It remains valid until the next call to
    //! `initialize` or `finalize`.
    //!
    //! @tparam Class A registered class.
    template<class Class>
    inline static vptr_type static_vptr;

    //! List of policies selected in a registry.
    //!
    //! `policy_list` is a Boost.Mp11 list containing the policies passed to the
    //! @ref registry clas template.
    //!
    //! @tparam Class A registered class.
    using policy_list = mp11::mp_list<Policy...>;

    //! Find a policy by category.
    //!
    //! `policy` searches for a policy that derives from the specified @ref
    //! Category. If none is found, it aliases to `void`. Otherwise, it aliases
    //! to the policy's `fn` meta-function, applied to the registry.
    //!
    //! @tparam A policy.
    template<class Category>
    using policy = typename detail::get_policy_aux<
        registry,
        mp11::mp_find_if_q<
            policy_list,
            mp11::mp_bind_front_q<
                mp11::mp_quote_trait<std::is_base_of>, Category>>>::type;

    //! Add or replace policies.
    //!
    //! `with` aliases to a registry with additional policies, overwriting any
    //! existing policies in the same category as the new ones.
    //!
    //! @tparam NewPolicies Models of @ref policies::Policy.
    template<class... NewPolicies>
    using with = boost::mp11::mp_apply<
        registry, typename detail::with_aux<policy_list, NewPolicies...>::type>;

    //! Remove policies.
    //!
    //! `without` aliases to a registry containing the same policies, except those
    //! that derive from `Categories`.
    //!
    //! @tparam Categories Models of @ref policies::PolicyCategory.
    template<class... Categories>
    using without = boost::mp11::mp_apply<
        registry,
        typename detail::without_aux<policy_list, Categories...>::type>;

    //! The registry's rtti policy.
    using rtti = policy<policies::rtti>;

    //! The registry's vptr policy if it contains one, or `void`.
    using vptr = policy<policies::vptr>;

    //! `true` if the registry has a vptr policy.
    static constexpr auto has_vptr = !std::is_same_v<vptr, void>;

    //! The registry's error_handler policy if it contains one, or `void`.
    using error_handler = policy<policies::error_handler>;

    //! `true` if the registry has an error_handler policy.
    static constexpr auto has_error_handler =
        !std::is_same_v<error_handler, void>;

    //! The registry's output policy if it contains one, or `void`.
    using output = policy<policies::output>;

    //! `true` if the registry has an output policy.
    static constexpr auto has_output = !std::is_same_v<output, void>;

    //! `true` if the registry has a deferred_static_rtti policy.
    static constexpr auto has_deferred_static_rtti =
        !std::is_same_v<policy<policies::deferred_static_rtti>, void>;

    //! `true` if the registry has a runtime_checks policy.
    static constexpr auto has_runtime_checks =
        !std::is_same_v<policy<policies::runtime_checks>, void>;

    //! `true` if the registry has an indirect_vptr policy.
    static constexpr auto has_indirect_vptr =
        !std::is_same_v<policy<policies::indirect_vptr>, void>;
};

template<class... Policies>
void registry<Policies...>::require_initialized() {
    if constexpr (registry::has_runtime_checks) {
        if (!initialized) {
            if constexpr (registry::has_error_handler) {
                error_handler::error(not_initialized());
            }

            abort();
        }
    }
}

template<class Registry, class Stream>
auto bad_call::write_aux(Stream& os, const char* subtype) const -> void {
    using namespace detail;
    using namespace policies;

    os << "invalid call to ";
    Registry::template policy<rtti>::type_name(method, os);
    os << "(";
    auto comma = "";

    for (auto ti : range{types, types + arity}) {
        os << comma;
        Registry::template policy<rtti>::type_name(ti, os);
        comma = ", ";
    }

    os << "): " << subtype;
}

template<class Registry, class Stream>
auto missing_class::write(Stream& os) const -> void {
    os << "unknown class ";
    Registry::rtti::type_name(type, os);
}

template<class Registry, class Stream>
auto missing_base::write(Stream& os) const -> void {
    os << "missing base ";
    Registry::rtti::type_name(base, os);
    os << " -<| ";
    Registry::rtti::type_name(derived, os);
}

template<class Registry, class Stream>
auto final_error::write(Stream& os) const -> void {
    os << "invalid call to final construct: static type = ";
    Registry::rtti::type_name(static_type, os);
    os << ", dynamic type = ";
    Registry::rtti::type_name(dynamic_type, os);
}

} // namespace boost::openmethod

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // BOOST_OPENMETHOD_REGISTRY_HPP
