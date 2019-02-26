struct IUnknown; // because windows.h is a garbage: https://developercommunity.visualstudio.com/content/problem/185399/error-c2760-in-combaseapih-with-windows-sdk-81-and.html
#include <foobar2000.h>
#include <wtsapi32.h>
#include <thread>
#include "helpers/AutoWTSFreeMemory.h"

// Declaration of your component's version information
// Since foobar2000 v1.0 having at least one of these in your DLL is mandatory to let the troubleshooter tell different versions of your component apart.
// Note that it is possible to declare multiple components within one DLL, but it's strongly recommended to keep only one declaration per DLL.
// As for 1.1, the version numbers are used by the component update finder to find updates; for that to work, you must have ONLY ONE declaration per DLL. If there are multiple declarations, the component is assumed to be outdated and a version number of "0" is assumed, to overwrite the component with whatever is currently on the site assuming that it comes with proper version numbers.
DECLARE_COMPONENT_VERSION("Autopause on RDS disconnect", "1.0", "Pauses/continues playback on RDS disconnect/connect");


// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or loading multiple instances of it.
//VALIDATE_COMPONENT_FILENAME("foo_rds_autopause.dll");

// Sample initquit implementation. See also: initquit class documentation in relevant header.

namespace {
    bool s_shutdownRequested = false;

    class SessionDeactivated: public main_thread_callback {
    public:
        virtual void callback_run() override {
            static_api_ptr_t<playback_control> playback;
            playback->pause(true);
#ifdef _DEBUG
            console::print("pausing the playback");
#endif
        }
    };

    void event_thread_main() {
        while (true) {
            DWORD resultingFlags = 0;
            if (::WTSWaitSystemEvent(WTS_CURRENT_SERVER_HANDLE, WTS_EVENT_STATECHANGE, &resultingFlags)) {
                if (WTS_EVENT_NONE == resultingFlags) {
                    //someone flushed the event queue. If it was us -- application is shutting down, and we should too
                    if (s_shutdownRequested) {
                        return;
                    }
                    // not us -- ignore
                } else {
                    helpers::AutoWTSFreeMemory<WTS_CONNECTSTATE_CLASS> mem;
                    DWORD dataSize = 0;
                    if (::WTSQuerySessionInformationW(
                        WTS_CURRENT_SERVER_HANDLE,
                        WTS_CURRENT_SESSION,
                        WTSConnectState,
                        reinterpret_cast<wchar_t**>(mem.p_ptr()),
                        &dataSize))
                    {
                        const WTS_CONNECTSTATE_CLASS currentSessionConnectionState = *mem.ptr();
                        if (currentSessionConnectionState == WTSDisconnected) {
                            main_thread_callback_manager::get()->add_callback(new service_impl_t<SessionDeactivated>);
                        }

                    } else {
                        //TODO: report error
                        ::OutputDebugStringA("WTSQuerySessionInformationW failed.\n");
                    }
                }
            } else {
                //TODO: report error
                ::OutputDebugStringA("WTSWaitSystemEvent failed.\n");
                return;
            }
        }
    }

}


class myinitquit : public initquit {
public:
    virtual void on_init() override {
        std::thread eventThread(event_thread_main);
        eventThread.detach();
#ifdef _DEBUG
        console::print("Autopause on RDS disconnect: on_init()");
#endif
    }

    virtual void on_quit() override {
        //signal our detector thread to quit
        s_shutdownRequested = true;
        DWORD dummy;
        ::WTSWaitSystemEvent(WTS_CURRENT_SERVER_HANDLE, WTS_EVENT_FLUSH, &dummy);
#ifdef _DEBUG
        ::OutputDebugStringA("Autopause on RDS disconnect: on_quit()\n");
#endif
    }
};

static initquit_factory_t<myinitquit> g_myinitquit_factory;