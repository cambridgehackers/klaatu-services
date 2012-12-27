namespace android {
#ifdef FSM_DEFINE_ENUMS
enum { EVENT_NONE=1,
    AUTHENTICATION_FAILURE_EVENT, CMD_ADD_OR_UPDATE_NETWORK, CMD_CONNECT_NETWORK, 
    CMD_DISABLE_NETWORK, CMD_DISCONNECT, CMD_ENABLE_BACKGROUND_SCAN, 
    CMD_ENABLE_NETWORK, CMD_ENABLE_RSSI_POLL, CMD_LOAD_DRIVER, 
    CMD_LOAD_DRIVER_FAILURE, CMD_LOAD_DRIVER_SUCCESS, CMD_REASSOCIATE, 
    CMD_RECONNECT, CMD_REMOVE_NETWORK, CMD_RSSI_POLL, 
    CMD_SELECT_NETWORK, CMD_START_DRIVER, CMD_START_SCAN, 
    CMD_START_SUPPLICANT, CMD_STOP_DRIVER, CMD_STOP_SUPPLICANT, 
    CMD_STOP_SUPPLICANT_FAILURE, CMD_STOP_SUPPLICANT_SUCCESS, CMD_UNLOAD_DRIVER, 
    CMD_UNLOAD_DRIVER_FAILURE, CMD_UNLOAD_DRIVER_SUCCESS, DHCP_FAILURE, 
    DHCP_SUCCESS, NETWORK_CONNECTION_EVENT, NETWORK_DISCONNECTION_EVENT, 
    SUP_CONNECTION_EVENT, SUP_DISCONNECTION_EVENT, SUP_SCAN_RESULTS_EVENT, 
    SUP_STATE_CHANGE_EVENT, MAX_WIFI_EVENT};
#endif
enum { STATE_NONE=1,
    CONNECT_MODE_STATE, CONNECTED_STATE, CONNECTING_STATE, 
    DEFER_STATE, DISCONNECTED_STATE, DISCONNECTING_STATE, 
    DRIVER_FAILED_STATE, DRIVER_LOADED_STATE, DRIVER_LOADING_STATE, 
    DRIVER_STARTED_STATE, DRIVER_STARTING_STATE, DRIVER_STOPPED_STATE, 
    DRIVER_STOPPING_STATE, DRIVER_UNLOADED_STATE, DRIVER_UNLOADING_STATE, 
    INITIAL_STATE, SCAN_MODE_STATE, SUPPLICANT_STARTED_STATE, 
    SUPPLICANT_STARTING_STATE, SUPPLICANT_STOPPING_STATE, UNUSED_STATE, 
    DEFAULT_STATE, STATE_MAX};
extern const char *sMessageToString[MAX_WIFI_EVENT];
#ifdef FSM_INITIALIZE_CODE
const char *sMessageToString[MAX_WIFI_EVENT];
STATE_TABLE_TYPE state_table[STATE_MAX];
void initstates(void)
{
    static STATE_TRANSITION TRA_Connect_Mode[] = {{CMD_CONNECT_NETWORK,DISCONNECTING_STATE}, {NETWORK_CONNECTION_EVENT,CONNECTING_STATE}, {NETWORK_DISCONNECTION_EVENT,DISCONNECTED_STATE}, {SUP_STATE_CHANGE_EVENT,DISCONNECTED_STATE}, {SUP_STATE_CHANGE_EVENT,DISCONNECTING_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Connected[] = {{0,CONNECTING_STATE}, {CMD_DISCONNECT,DISCONNECTING_STATE}, {DHCP_FAILURE,DISCONNECTING_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Connecting[] = {{0,DISCONNECTING_STATE}, {CMD_START_SCAN,DEFER_STATE}, {DHCP_SUCCESS,CONNECTED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Disconnected[] = {{CMD_START_SCAN,SCAN_MODE_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Disconnecting[] = {{SUP_STATE_CHANGE_EVENT,DISCONNECTED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Loaded[] = {{CMD_START_SUPPLICANT,SUPPLICANT_STARTING_STATE}, {CMD_UNLOAD_DRIVER,DRIVER_UNLOADING_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Loading[] = {{CMD_LOAD_DRIVER,DEFER_STATE}, {CMD_LOAD_DRIVER_FAILURE,DRIVER_FAILED_STATE}, {CMD_LOAD_DRIVER_SUCCESS,DRIVER_LOADED_STATE}, {CMD_START_DRIVER,DEFER_STATE}, {CMD_START_SUPPLICANT,DEFER_STATE}, {CMD_STOP_DRIVER,DEFER_STATE}, {CMD_STOP_SUPPLICANT,DEFER_STATE}, {CMD_UNLOAD_DRIVER,DEFER_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Started[] = {{0,DISCONNECTED_STATE}, {0,SCAN_MODE_STATE}, {CMD_STOP_DRIVER,DRIVER_STOPPING_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Starting[] = {{AUTHENTICATION_FAILURE_EVENT,DEFER_STATE}, {CMD_DISCONNECT,DEFER_STATE}, {CMD_REASSOCIATE,DEFER_STATE}, {CMD_RECONNECT,DEFER_STATE}, {CMD_START_DRIVER,DEFER_STATE}, {CMD_START_SCAN,DEFER_STATE}, {CMD_STOP_DRIVER,DEFER_STATE}, {NETWORK_CONNECTION_EVENT,DEFER_STATE}, {NETWORK_DISCONNECTION_EVENT,DEFER_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Stopped[] = {{AUTHENTICATION_FAILURE_EVENT,DEFER_STATE}, {CMD_DISCONNECT,DEFER_STATE}, {CMD_REASSOCIATE,DEFER_STATE}, {CMD_RECONNECT,DEFER_STATE}, {CMD_STOP_DRIVER,DEFER_STATE}, {NETWORK_CONNECTION_EVENT,DEFER_STATE}, {NETWORK_DISCONNECTION_EVENT,DEFER_STATE}, {SUP_STATE_CHANGE_EVENT,DRIVER_STARTED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Stopping[] = {{AUTHENTICATION_FAILURE_EVENT,DEFER_STATE}, {CMD_DISCONNECT,DEFER_STATE}, {CMD_REASSOCIATE,DEFER_STATE}, {CMD_RECONNECT,DEFER_STATE}, {CMD_START_DRIVER,DEFER_STATE}, {CMD_START_SCAN,DEFER_STATE}, {CMD_STOP_DRIVER,DEFER_STATE}, {NETWORK_CONNECTION_EVENT,DEFER_STATE}, {NETWORK_DISCONNECTION_EVENT,DEFER_STATE}, {SUP_STATE_CHANGE_EVENT,DRIVER_STOPPED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Unloaded[] = {{CMD_LOAD_DRIVER,DRIVER_LOADING_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Driver_Unloading[] = {{CMD_LOAD_DRIVER,DEFER_STATE}, {CMD_START_DRIVER,DEFER_STATE}, {CMD_START_SUPPLICANT,DEFER_STATE}, {CMD_STOP_DRIVER,DEFER_STATE}, {CMD_STOP_SUPPLICANT,DEFER_STATE}, {CMD_UNLOAD_DRIVER,DEFER_STATE}, {CMD_UNLOAD_DRIVER_FAILURE,DRIVER_FAILED_STATE}, {CMD_UNLOAD_DRIVER_SUCCESS,DRIVER_UNLOADED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Initial[] = {{0,DRIVER_LOADED_STATE}, {0,DRIVER_UNLOADED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Scan_Mode[] = {{CMD_START_SCAN,DISCONNECTED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Supplicant_Started[] = {{CMD_STOP_SUPPLICANT,SUPPLICANT_STOPPING_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Supplicant_Starting[] = {{CMD_LOAD_DRIVER,DEFER_STATE}, {CMD_START_DRIVER,DEFER_STATE}, {CMD_START_SUPPLICANT,DEFER_STATE}, {CMD_STOP_DRIVER,DEFER_STATE}, {CMD_STOP_SUPPLICANT,DEFER_STATE}, {CMD_UNLOAD_DRIVER,DEFER_STATE}, {SUP_CONNECTION_EVENT,DRIVER_STARTED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Supplicant_Stopping[] = {{CMD_STOP_SUPPLICANT_FAILURE,DRIVER_LOADED_STATE}, {0,0} };
    static STATE_TRANSITION TRA_Unused[] = {{CMD_ADD_OR_UPDATE_NETWORK,DEFER_STATE}, {CMD_DISABLE_NETWORK,DEFER_STATE}, {CMD_ENABLE_BACKGROUND_SCAN,DEFER_STATE}, {CMD_ENABLE_NETWORK,DEFER_STATE}, {CMD_ENABLE_RSSI_POLL,DEFER_STATE}, {CMD_REMOVE_NETWORK,DEFER_STATE}, {CMD_RSSI_POLL,DEFER_STATE}, {CMD_SELECT_NETWORK,DEFER_STATE}, {CMD_STOP_SUPPLICANT_SUCCESS,DEFER_STATE}, {SUP_SCAN_RESULTS_EVENT,DEFER_STATE}, {0,0} };
    static STATE_TRANSITION TRA_default[] = {{CMD_STOP_SUPPLICANT,SUPPLICANT_STOPPING_STATE}, {SUP_DISCONNECTION_EVENT,DRIVER_LOADED_STATE}, {0,0} };

    state_table[CONNECT_MODE_STATE].name = "Connect_Mode";
    state_table[CONNECT_MODE_STATE].tran = TRA_Connect_Mode;
    state_table[CONNECTED_STATE].name = "Connected";
    state_table[CONNECTED_STATE].tran = TRA_Connected;
    state_table[CONNECTING_STATE].name = "Connecting";
    state_table[CONNECTING_STATE].tran = TRA_Connecting;
    state_table[DEFER_STATE].name = "DEFER";
    state_table[DISCONNECTED_STATE].name = "Disconnected";
    state_table[DISCONNECTED_STATE].tran = TRA_Disconnected;
    state_table[DISCONNECTING_STATE].name = "Disconnecting";
    state_table[DISCONNECTING_STATE].tran = TRA_Disconnecting;
    state_table[DRIVER_FAILED_STATE].name = "Driver_Failed";
    state_table[DRIVER_LOADED_STATE].name = "Driver_Loaded";
    state_table[DRIVER_LOADED_STATE].tran = TRA_Driver_Loaded;
    state_table[DRIVER_LOADING_STATE].name = "Driver_Loading";
    state_table[DRIVER_LOADING_STATE].tran = TRA_Driver_Loading;
    state_table[DRIVER_STARTED_STATE].name = "Driver_Started";
    state_table[DRIVER_STARTED_STATE].tran = TRA_Driver_Started;
    state_table[DRIVER_STARTING_STATE].name = "Driver_Starting";
    state_table[DRIVER_STARTING_STATE].tran = TRA_Driver_Starting;
    state_table[DRIVER_STOPPED_STATE].name = "Driver_Stopped";
    state_table[DRIVER_STOPPED_STATE].tran = TRA_Driver_Stopped;
    state_table[DRIVER_STOPPING_STATE].name = "Driver_Stopping";
    state_table[DRIVER_STOPPING_STATE].tran = TRA_Driver_Stopping;
    state_table[DRIVER_UNLOADED_STATE].name = "Driver_Unloaded";
    state_table[DRIVER_UNLOADED_STATE].tran = TRA_Driver_Unloaded;
    state_table[DRIVER_UNLOADING_STATE].name = "Driver_Unloading";
    state_table[DRIVER_UNLOADING_STATE].tran = TRA_Driver_Unloading;
    state_table[INITIAL_STATE].name = "Initial";
    state_table[INITIAL_STATE].tran = TRA_Initial;
    state_table[SCAN_MODE_STATE].name = "Scan_Mode";
    state_table[SCAN_MODE_STATE].tran = TRA_Scan_Mode;
    state_table[SUPPLICANT_STARTED_STATE].name = "Supplicant_Started";
    state_table[SUPPLICANT_STARTED_STATE].tran = TRA_Supplicant_Started;
    state_table[SUPPLICANT_STARTING_STATE].name = "Supplicant_Starting";
    state_table[SUPPLICANT_STARTING_STATE].tran = TRA_Supplicant_Starting;
    state_table[SUPPLICANT_STOPPING_STATE].name = "Supplicant_Stopping";
    state_table[SUPPLICANT_STOPPING_STATE].tran = TRA_Supplicant_Stopping;
    state_table[UNUSED_STATE].name = "Unused";
    state_table[UNUSED_STATE].tran = TRA_Unused;
    state_table[DEFAULT_STATE].name = "default";
    state_table[DEFAULT_STATE].tran = TRA_default;
    sMessageToString[AUTHENTICATION_FAILURE_EVENT] = "AUTHENTICATION_FAILURE_EVENT";
    sMessageToString[CMD_ADD_OR_UPDATE_NETWORK] = "CMD_ADD_OR_UPDATE_NETWORK";
    sMessageToString[CMD_CONNECT_NETWORK] = "CMD_CONNECT_NETWORK";
    sMessageToString[CMD_DISABLE_NETWORK] = "CMD_DISABLE_NETWORK";
    sMessageToString[CMD_DISCONNECT] = "CMD_DISCONNECT";
    sMessageToString[CMD_ENABLE_BACKGROUND_SCAN] = "CMD_ENABLE_BACKGROUND_SCAN";
    sMessageToString[CMD_ENABLE_NETWORK] = "CMD_ENABLE_NETWORK";
    sMessageToString[CMD_ENABLE_RSSI_POLL] = "CMD_ENABLE_RSSI_POLL";
    sMessageToString[CMD_LOAD_DRIVER] = "CMD_LOAD_DRIVER";
    sMessageToString[CMD_LOAD_DRIVER_FAILURE] = "CMD_LOAD_DRIVER_FAILURE";
    sMessageToString[CMD_LOAD_DRIVER_SUCCESS] = "CMD_LOAD_DRIVER_SUCCESS";
    sMessageToString[CMD_REASSOCIATE] = "CMD_REASSOCIATE";
    sMessageToString[CMD_RECONNECT] = "CMD_RECONNECT";
    sMessageToString[CMD_REMOVE_NETWORK] = "CMD_REMOVE_NETWORK";
    sMessageToString[CMD_RSSI_POLL] = "CMD_RSSI_POLL";
    sMessageToString[CMD_SELECT_NETWORK] = "CMD_SELECT_NETWORK";
    sMessageToString[CMD_START_DRIVER] = "CMD_START_DRIVER";
    sMessageToString[CMD_START_SCAN] = "CMD_START_SCAN";
    sMessageToString[CMD_START_SUPPLICANT] = "CMD_START_SUPPLICANT";
    sMessageToString[CMD_STOP_DRIVER] = "CMD_STOP_DRIVER";
    sMessageToString[CMD_STOP_SUPPLICANT] = "CMD_STOP_SUPPLICANT";
    sMessageToString[CMD_STOP_SUPPLICANT_FAILURE] = "CMD_STOP_SUPPLICANT_FAILURE";
    sMessageToString[CMD_STOP_SUPPLICANT_SUCCESS] = "CMD_STOP_SUPPLICANT_SUCCESS";
    sMessageToString[CMD_UNLOAD_DRIVER] = "CMD_UNLOAD_DRIVER";
    sMessageToString[CMD_UNLOAD_DRIVER_FAILURE] = "CMD_UNLOAD_DRIVER_FAILURE";
    sMessageToString[CMD_UNLOAD_DRIVER_SUCCESS] = "CMD_UNLOAD_DRIVER_SUCCESS";
    sMessageToString[DHCP_FAILURE] = "DHCP_FAILURE";
    sMessageToString[DHCP_SUCCESS] = "DHCP_SUCCESS";
    sMessageToString[NETWORK_CONNECTION_EVENT] = "NETWORK_CONNECTION_EVENT";
    sMessageToString[NETWORK_DISCONNECTION_EVENT] = "NETWORK_DISCONNECTION_EVENT";
    sMessageToString[SUP_CONNECTION_EVENT] = "SUP_CONNECTION_EVENT";
    sMessageToString[SUP_DISCONNECTION_EVENT] = "SUP_DISCONNECTION_EVENT";
    sMessageToString[SUP_SCAN_RESULTS_EVENT] = "SUP_SCAN_RESULTS_EVENT";
    sMessageToString[SUP_STATE_CHANGE_EVENT] = "SUP_STATE_CHANGE_EVENT";
}

#endif

#ifdef FSM_ACTION_CODE
#define addstateitem(command, aenter, aprocess, aexit, parent) \
    mStateMap[command].mName = #command; \
    mStateMap[command].mParent = parent; \
    mStateMap[command].mEnter = aenter; \
    mStateMap[command].mProcess = aprocess; \
    mStateMap[command].mExit = aexit;

class WifiStateMachineActions: public WifiStateMachine {
public:
stateprocess_t sm_default_process(Message *);
stateprocess_t Connect_Mode_process(Message *);
void Connected_enter(void);
stateprocess_t Connected_process(Message *);
void Connected_exit(void);
void Disconnected_enter(void);
stateprocess_t Disconnected_process(Message *);
void Disconnected_exit(void);
stateprocess_t Disconnecting_process(Message *);
stateprocess_t Driver_Failed_process(Message *);
void Driver_Loading_enter(void);
void Driver_Started_enter(void);
stateprocess_t Driver_Started_process(Message *);
stateprocess_t Driver_Stopping_process(Message *);
void Driver_Unloading_enter(void);
stateprocess_t Scan_Mode_process(Message *);
stateprocess_t Supplicant_Started_process(Message *);
void Supplicant_Started_exit(void);
stateprocess_t Supplicant_Starting_process(Message *);
stateprocess_t Supplicant_Stopping_process(Message *);
};
void ADD_ITEMS(State *mStateMap) {
    addstateitem(CONNECT_MODE_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Connect_Mode_process), 0, DRIVER_STARTED_STATE);
    addstateitem(CONNECTED_STATE, static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Connected_enter),static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Connected_process),static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Connected_exit), CONNECT_MODE_STATE);
    addstateitem(CONNECTING_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::sm_default_process), 0, CONNECT_MODE_STATE);
    addstateitem(DISCONNECTED_STATE, static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Disconnected_enter),static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Disconnected_process),static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Disconnected_exit), CONNECT_MODE_STATE);
    addstateitem(DISCONNECTING_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Disconnecting_process), 0, CONNECT_MODE_STATE);
    addstateitem(DRIVER_FAILED_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Driver_Failed_process), 0, DRIVER_UNLOADED_STATE);
    addstateitem(DRIVER_LOADED_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::sm_default_process), 0, 0);
    addstateitem(DRIVER_LOADING_STATE, static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Driver_Loading_enter),static_cast<PROCESS_PROTO>(&WifiStateMachineActions::sm_default_process), 0, 0);
    addstateitem(DRIVER_STARTED_STATE, static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Driver_Started_enter),static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Driver_Started_process), 0, SUPPLICANT_STARTED_STATE);
    addstateitem(DRIVER_STOPPED_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::sm_default_process), 0, SUPPLICANT_STARTED_STATE);
    addstateitem(DRIVER_STOPPING_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Driver_Stopping_process), 0, SUPPLICANT_STARTED_STATE);
    addstateitem(DRIVER_UNLOADED_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::sm_default_process), 0, 0);
    addstateitem(DRIVER_UNLOADING_STATE, static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Driver_Unloading_enter),static_cast<PROCESS_PROTO>(&WifiStateMachineActions::sm_default_process), 0, 0);
    addstateitem(SCAN_MODE_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Scan_Mode_process), 0, DRIVER_STARTED_STATE);
    addstateitem(SUPPLICANT_STARTED_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Supplicant_Started_process),static_cast<ENTER_EXIT_PROTO>(&WifiStateMachineActions::Supplicant_Started_exit), 0);
    addstateitem(SUPPLICANT_STARTING_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Supplicant_Starting_process), 0, 0);
    addstateitem(SUPPLICANT_STOPPING_STATE, 0,static_cast<PROCESS_PROTO>(&WifiStateMachineActions::Supplicant_Stopping_process), 0, 0);
    addstateitem(DEFAULT_STATE, 0, 0, 0, 0);
}

#endif
} /* namespace android */
