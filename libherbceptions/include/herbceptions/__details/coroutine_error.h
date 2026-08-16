#pragma once
/*
coroutine_error — the coroutine-frame carrier for herbception errors.

It has the same {domain, code} layout as std::error, but unlike std::error it
is movable and nullable (the default/null state means "no error"), so it can
be stored in a coroutine frame and moved out of it: the promise's
unhandled_herbception(std::coroutine_error) stores the error in the frame, and
the awaiter later moves it out and rethrows it. The compiler fabricates it for
the unhandled_herbception call and passes an existing std::error value through
unchanged.

Copied from the stderror-prototype (herbceptions/coroutines/coroutine_error.h).

This header is included at the end of herbceptions/error (which defines the
core std::error types this carrier needs), so it does not include error itself;
that also avoids the circular include. Include "herbceptions/error" instead.
*/

namespace std
{
/*
A carrier type for error
*/
class coroutine_error
{
    ::std::error_domain_singleton const* __domain_opaque;
    ::std::size_t __code_opaque;
public:
    coroutine_error() noexcept = delete;
    coroutine_error(coroutine_error const&) = delete;
    coroutine_error& operator=(coroutine_error const&) = delete;
    constexpr coroutine_error(coroutine_error&& __other) noexcept:
        __domain_opaque{__other.__domain_opaque},__code_opaque{__other.__code_opaque}
    {
        __other.__domain_opaque = nullptr;
        __other.__code_opaque = 0;
    }
    constexpr coroutine_error& operator=(coroutine_error&& __other) noexcept
    {
        if(this == __builtin_addressof(__other))
        {
            return *this;
        }
        if(__domain_opaque)
        {
            auto __docleanup{__domain_opaque->do_cleanup};
            if (__docleanup)
            {
                __docleanup(__code_opaque);
            }
        }
        this->__domain_opaque = __other.__domain_opaque;
        this->__code_opaque = __other.__code_opaque;
        __other.__domain_opaque = nullptr;
        __other.__code_opaque = 0;
        return *this;
    }
    constexpr ~coroutine_error()
    {
        if(__domain_opaque)
        {
            auto __docleanup{__domain_opaque->do_cleanup};
            if (__docleanup)
            {
                __docleanup(__code_opaque);
            }
        }
    }
    constexpr operator bool() const noexcept
    {
        return __domain_opaque;
    }
};

}
