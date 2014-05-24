#include <wayward/support/event_loop.hpp>
#include <wayward/support/event_loop_private.hpp>
#include <wayward/support/fiber.hpp>

namespace wayward {
    struct EventLoop::Private {
      event_base* base = nullptr;
      Fiber* running_in_fiber = nullptr;

      Private() {
        base = event_base_new();
      }

      ~Private() {
        // TODO: Run all finalizers!
        event_base_free(base);
      }
    };

    EventLoop::~EventLoop() {}

    EventLoop::EventLoop() : p_(new Private) {}

    void EventLoop::run() {
      event_base_dispatch(p_->base);
    }

    void EventLoop::resume() {

    }

    void EventLoop::terminate() {

    }

    void* EventLoop::native_handle() const {
      return p_->base;
    }

    namespace {
      using FDEventCallback = EventLoop::FDEventCallback;

      struct EventHandle_libevent : IEventHandle {
        event* ev;
        ~EventHandle_libevent() {
          event_del(ev);
        }
      };

      struct FDEventHandle_libevent : EventHandle_libevent {
        FDEventCallback callback;

        void handle_event(evutil_socket_t fd, short ev) {
          callback(fd, ev);
        }

        static void handle_event_cb(evutil_socket_t fd, short ev, void* userdata) {
          auto handle = static_cast<FDEventHandle_libevent*>(userdata);
          handle->handle_event(fd, ev);
        }
      };

      struct TimeoutEventHandle_libevent : EventHandle_libevent {
        std::function<void()> callback;

        void handle_event(evutil_socket_t fd, short ev) {
          callback();
        }

        static void handle_event_cb(evutil_socket_t fd, short ev, void* userdata) {
          auto handle = static_cast<TimeoutEventHandle_libevent*>(userdata);
          handle->handle_event(fd, ev);
        }
      };
    }

    std::unique_ptr<IEventHandle> EventLoop::add_file_descriptor(int fd, FDEvents events, FDEventCallback callback) {
      evutil_make_socket_nonblocking(fd);
      auto handle = std::unique_ptr<FDEventHandle_libevent>(new FDEventHandle_libevent);
      event* ev = event_new(p_->base, fd, (short)events, FDEventHandle_libevent::handle_event_cb, handle.get());
      handle->ev = ev;
      handle->callback = std::move(callback);
      event_add(ev, nullptr);
      return std::move(handle);
    }

    std::unique_ptr<IEventHandle> EventLoop::call_in(DateTimeInterval interval, std::function<void()> callback, bool repeat) {
      auto handle = std::unique_ptr<TimeoutEventHandle_libevent>(new TimeoutEventHandle_libevent);
      short events = EV_TIMEOUT;
      if (repeat) events |= EV_PERSIST;
      event* ev = event_new(p_->base, -1, events, TimeoutEventHandle_libevent::handle_event_cb, handle.get());
      handle->ev = ev;
      handle->callback = std::move(callback);
      struct timeval tv = interval.to_timeval();
      event_add(ev, &tv);
      return std::move(handle);
    }

}