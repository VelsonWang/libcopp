#ifndef COPP_FUTURE_FUTURE_H
#define COPP_FUTURE_FUTURE_H

#pragma once

#include <list>

#include "context.h"
#include "poll.h"

namespace copp {
    namespace future {
        template <class T, class TPTR = typename poll_storage_select_ptr_t<T>::type>
        class LIBCOPP_COPP_API_HEAD_ONLY future_t {
        public:
#if defined(UTIL_CONFIG_COMPILER_CXX_ALIAS_TEMPLATES) && UTIL_CONFIG_COMPILER_CXX_ALIAS_TEMPLATES
            using self_type    = future_t<T, TPTR>;
            using poll_type    = poll_t<T, TPTR>;
            using storage_type = typename poll_type::storage_type;
            using value_type   = typename poll_type::value_type;
            using ptr_type     = typename poll_type::ptr_type;
#else
            typedef future_t<T, TPTR>                self_type;
            typedef poll_t<T, TPTR>                  poll_type;
            typedef typename poll_type::storage_type storage_type;
            typedef typename poll_type::value_type   value_type;
            typedef typename poll_type::ptr_type     ptr_type;
#endif

            struct waker_t {
                self_type *self;
            };

        private:
            // future can not be copy or moved.
            future_t(const future_t &) UTIL_CONFIG_DELETED_FUNCTION;
            future_t &operator=(const future_t &) UTIL_CONFIG_DELETED_FUNCTION;
            future_t(future_t &&) UTIL_CONFIG_DELETED_FUNCTION;
            future_t &operator=(future_t &&) UTIL_CONFIG_DELETED_FUNCTION;

#if defined(UTIL_CONFIG_COMPILER_CXX_STATIC_ASSERT) && UTIL_CONFIG_COMPILER_CXX_STATIC_ASSERT
        private:
            struct _test_context_functor_t {
                // template <class U>
                // int operator()(U *);

                template <class U>
                bool operator()(context_t<U> *);
            };
#endif

        protected:
            template <class TSELF, class TCONTEXT>
            static void poll(TSELF &self, TCONTEXT &&ctx) {
#if defined(UTIL_CONFIG_COMPILER_CXX_STATIC_ASSERT) && UTIL_CONFIG_COMPILER_CXX_STATIC_ASSERT
                typedef typename std::decay<TCONTEXT>::type decay_context_type;
                static_assert(std::is_same<bool, COPP_RETURN_VALUE_DECAY(_test_context_functor_t, decay_context_type *)>::value,
                              "ctx must be drive of context_t");
                static_assert(std::is_base_of<self_type, typename std::decay<TSELF>::type>::value,
                              "self must be drive of future_t<T, TPTR>");
#endif
                if (self.is_ready()) {
                    return;
                }

                // Set waker first, and then context can be moved or copyed in private data callback
                // If two or more context poll the same future, we just use the last one
                if (!ctx.get_wake_fn()) {
                    ctx.set_wake_fn(wake_future_t<TSELF>(self));
                    self.set_ctx_waker(std::forward<TCONTEXT>(ctx));
                }

                ctx.poll(self);

                if (self.is_ready() && self.clear_ctx_waker_) {
                    self.clear_ctx_waker();
                }
            }

        public:
            future_t() {}
            ~future_t() { clear_ctx_waker(); }

            inline bool is_ready() const UTIL_CONFIG_NOEXCEPT { return poll_data_.is_ready(); }

            inline bool is_pending() const UTIL_CONFIG_NOEXCEPT { return poll_data_.is_pending(); }

            template <class TCONTEXT>
            void poll(TCONTEXT &&ctx) {
                poll(*this, std::forward<TCONTEXT>(ctx));
            }

            inline const value_type *data() const UTIL_CONFIG_NOEXCEPT {
                if (!is_ready()) {
                    return NULL;
                }

                return poll_data_.data();
            }

            inline value_type *data() UTIL_CONFIG_NOEXCEPT {
                if (!is_ready()) {
                    return NULL;
                }

                return poll_data_.data();
            }

            inline const ptr_type & raw_ptr() const UTIL_CONFIG_NOEXCEPT { return poll_data_.raw_ptr(); }
            inline ptr_type &       raw_ptr() UTIL_CONFIG_NOEXCEPT { return poll_data_.raw_ptr(); }
            inline const poll_type &poll_data() const UTIL_CONFIG_NOEXCEPT { return poll_data_; }
            inline poll_type &      poll_data() UTIL_CONFIG_NOEXCEPT { return poll_data_; }


            inline void clear_ctx_waker() {
                if (clear_ctx_waker_) {
                    clear_ctx_waker_();
                    clear_ctx_waker_ = NULL;
                }
            }

            template<class TCONTEXT>
            inline void set_ctx_waker(TCONTEXT&& ctx) {
                clear_ctx_waker();
                clear_ctx_waker_ = clear_context_waker_t<typename std::decay<TCONTEXT>::type>(ctx); 
            }

        protected:
            template<class TCONTEXT>
            struct clear_context_waker_t {
                TCONTEXT* context;
                clear_context_waker_t(TCONTEXT& ctx): context(&ctx) {}

                void operator()() {
                    if (likely(context)) {
                        context->set_wake_fn(NULL);
                    }
                }
            };

        private:
            template<class TSELF>
            struct wake_future_t {
                TSELF* self;
                wake_future_t(TSELF& s) : self(&s) {}

                template <class TCONTEXT>
                void operator()(TCONTEXT &&ctx) {
                    if (likely(self)) {
                        self->poll(std::forward<TCONTEXT>(ctx));
                    }
                }
            };

        private:
            std::function<void ()> clear_ctx_waker_;
            poll_type              poll_data_;
        };
    } // namespace future
} // namespace copp

#endif