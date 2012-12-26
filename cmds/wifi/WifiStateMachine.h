/*
  Wifi State Machine

  Matches pretty closely the state machine in WifiStateMachine.java
  Please note that this class is a Thread
*/

#ifndef _WIFI_STATE_MACHINE_H
#define _WIFI_STATE_MACHINE_H

#include <utils/String8.h>
#include <wifi/IWifiClient.h>
#include "StateMachine.h"

namespace android {
class WifiService;
class WifiStateMachine : public StateMachine 
{
public:
    WifiStateMachine(const char *interface, WifiService *servicep);
    void           invoke_enter(ENTER_EXIT_PROTO fn);
    stateprocess_t invoke_process(PROCESS_PROTO fn, Message *m, STATE_TRANSITION *t);

    void           enqueue_network_update(const ConfiguredStation& cs);
    /* The WifiMonitor watches for supplicant messages about wifi
     state and posts them to the state machine.  It runs in its own thread */
    void           Register(const sp<IWifiClient>& client, int flags);
    int            request_wifi(int request);
    bool           process_indication(void);
    enum { WIFI_LOAD_DRIVER = 1, WIFI_UNLOAD_DRIVER, WIFI_IS_DRIVER_LOADED,
        WIFI_START_SUPPLICANT, WIFI_STOP_SUPPLICANT,
        WIFI_CONNECT_SUPPLICANT, WIFI_CLOSE_SUPPLICANT, WIFI_WAIT_EVENT,
        DHCP_STOP, DHCP_DO_REQUEST};

protected:
    int            findIndexByNetworkId(int network_id);
    void           handleConnected(const char *);
    void           handleSupplicantStateChange(Message *);
    String8        doWifiStringCommand(const char *fmt, va_list args);
    String8        doWifiStringCommand(const char *fmt, ...);
    bool           doWifiBooleanCommand(const char *fmt, ...);
    void           readNetworkVariables(ConfiguredStation& station);
    void           setStatus(const char *command, int network_id, ConfiguredStation::Status astatus);
    virtual const char *msgStr(int msg_id);

    String8        mInterface;
    bool           mIsScanMode;
    bool           mEnableRssiPolling;
    bool           mEnableBackgroundScan;
    bool           mScanResultIsPending;
    int            mSupplicantRestartCount;
    WifiService    *mService;

    // These elements can be accessed by an outside thread
    // so they must all be read-protected
    mutable Mutex              mReadLock; 
    WifiInformation            mWifiInformation;   // Information about the current network
    // TODO:  Probably need a Mutex lock
    Vector<ConfiguredStation>  mStationsConfig;
    void                       setInterfaceState(int astate);
    void                       flushDnsCache();
    String8                    ncommand(const char *fmt, ...);
    void                       disable_interface(void);
private:
    mutable Mutex              mLock;     // Protects the response queue
    mutable Condition          mCondition;
    int                        mFd;
    Vector<String8>            mResponseQueue;
    int                        mSequenceNumber;
    char                       indication_buf[1024];
    int                        indication_start;
};

};  // namespace android

#define FSM_DEFINE_ENUMS
#include "wifistates.h"

#endif // _WIFI_STATE_MACHINE_H
