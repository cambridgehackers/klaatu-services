/*
 The WifiMonitor watches for supplicant messages about wifi
 state and posts them to the state machine.  It runs in its 
 own thread
*/

#ifndef _WIFI_MONITOR_H
#define _WIFI_MONITOR_H

namespace android {

class StateMachine;

class WifiMonitor {
public:
    WifiMonitor(StateMachine *);
    void startRunning();
    int monitorThreadFunction();

private:
    bool handleSupplicantEvent(const char *);
    void handleStateChange(const char *);
    void handleConnected(const char *);

private:
    StateMachine *mStateMachine;
};

};  // namespace android

#endif // _WIFI_MONITOR_H
