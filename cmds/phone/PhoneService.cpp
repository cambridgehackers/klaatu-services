/*
  PhoneService
*/

#define LOG_NDEBUG 0
#define LOG_TAG "KlaatuPhone"

#include "PhoneService.h"

#include <phone/IPhoneClient.h>
#include <phone/PhoneClient.h>

#include <utils/threads.h>
#include <cutils/sockets.h>
#include <arpa/inet.h>
#include <telephony/ril.h>

namespace android {

// ---------------------------------------------------------------------------

static const char * rilMessageStr(int message)
{
    switch (message) {
    case RIL_REQUEST_GET_SIM_STATUS : return "GET_SIM_STATUS";
    case RIL_REQUEST_ENTER_SIM_PIN : return "ENTER_SIM_PIN";
    case RIL_REQUEST_ENTER_SIM_PUK : return "ENTER_SIM_PUK";
    case RIL_REQUEST_ENTER_SIM_PIN2 : return "ENTER_SIM_PIN2";
    case RIL_REQUEST_ENTER_SIM_PUK2 : return "ENTER_SIM_PUK2";
    case RIL_REQUEST_CHANGE_SIM_PIN : return "CHANGE_SIM_PIN";
    case RIL_REQUEST_CHANGE_SIM_PIN2 : return "CHANGE_SIM_PIN2";
    case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION : return "ENTER_NETWORK_DEPERSONALIZATION";
    case RIL_REQUEST_GET_CURRENT_CALLS : return "GET_CURRENT_CALLS";
    case RIL_REQUEST_DIAL : return "DIAL";
    case RIL_REQUEST_GET_IMSI : return "GET_IMSI";
    case RIL_REQUEST_HANGUP : return "HANGUP";
    case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND : return "HANGUP_WAITING_OR_BACKGROUND";
    case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND : return "HANGUP_FOREGROUND_RESUME_BACKGROUND";
    case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE : return "SWITCH_WAITING_OR_HOLDING_AND_ACTIVE";
//    case RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE : return "SWITCH_HOLDING_AND_ACTIVE";
    case RIL_REQUEST_CONFERENCE : return "CONFERENCE";
    case RIL_REQUEST_UDUB : return "UDUB";
    case RIL_REQUEST_LAST_CALL_FAIL_CAUSE : return "LAST_CALL_FAIL_CAUSE";
    case RIL_REQUEST_SIGNAL_STRENGTH : return "SIGNAL_STRENGTH";
    case RIL_REQUEST_VOICE_REGISTRATION_STATE : return "VOICE_REGISTRATION_STATE";
    case RIL_REQUEST_DATA_REGISTRATION_STATE : return "DATA_REGISTRATION_STATE";
    case RIL_REQUEST_OPERATOR : return "OPERATOR";
    case RIL_REQUEST_RADIO_POWER : return "RADIO_POWER";
    case RIL_REQUEST_DTMF : return "DTMF";
    case RIL_REQUEST_SEND_SMS : return "SEND_SMS";
    case RIL_REQUEST_SEND_SMS_EXPECT_MORE : return "SEND_SMS_EXPECT_MORE";
    case RIL_REQUEST_SETUP_DATA_CALL : return "SETUP_DATA_CALL";
    case RIL_REQUEST_SIM_IO : return "SIM_IO";
    case RIL_REQUEST_SEND_USSD : return "SEND_USSD";
    case RIL_REQUEST_CANCEL_USSD : return "CANCEL_USSD";
    case RIL_REQUEST_GET_CLIR : return "GET_CLIR";
    case RIL_REQUEST_SET_CLIR : return "SET_CLIR";
    case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS : return "QUERY_CALL_FORWARD_STATUS";
    case RIL_REQUEST_SET_CALL_FORWARD : return "SET_CALL_FORWARD";
    case RIL_REQUEST_QUERY_CALL_WAITING : return "QUERY_CALL_WAITING";
    case RIL_REQUEST_SET_CALL_WAITING : return "SET_CALL_WAITING";
    case RIL_REQUEST_SMS_ACKNOWLEDGE : return "SMS_ACKNOWLEDGE";
    case RIL_REQUEST_GET_IMEI : return "GET_IMEI";
    case RIL_REQUEST_GET_IMEISV : return "GET_IMEISV";
    case RIL_REQUEST_ANSWER : return "ANSWER";
    case RIL_REQUEST_DEACTIVATE_DATA_CALL : return "DEACTIVATE_DATA_CALL";
    case RIL_REQUEST_QUERY_FACILITY_LOCK : return "QUERY_FACILITY_LOCK";
    case RIL_REQUEST_SET_FACILITY_LOCK : return "SET_FACILITY_LOCK";
    case RIL_REQUEST_CHANGE_BARRING_PASSWORD : return "CHANGE_BARRING_PASSWORD";
    case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE : return "QUERY_NETWORK_SELECTION_MODE";
    case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC : return "SET_NETWORK_SELECTION_AUTOMATIC";
    case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL : return "SET_NETWORK_SELECTION_MANUAL";
    case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS : return "QUERY_AVAILABLE_NETWORKS";
    case RIL_REQUEST_DTMF_START : return "DTMF_START";
    case RIL_REQUEST_DTMF_STOP : return "DTMF_STOP";
    case RIL_REQUEST_BASEBAND_VERSION : return "BASEBAND_VERSION";
    case RIL_REQUEST_SEPARATE_CONNECTION : return "SEPARATE_CONNECTION";
    case RIL_REQUEST_SET_MUTE : return "SET_MUTE";
    case RIL_REQUEST_GET_MUTE : return "GET_MUTE";
    case RIL_REQUEST_QUERY_CLIP : return "QUERY_CLIP";
    case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE : return "LAST_DATA_CALL_FAIL_CAUSE";
    case RIL_REQUEST_DATA_CALL_LIST : return "DATA_CALL_LIST";
    case RIL_REQUEST_RESET_RADIO : return "RESET_RADIO";
    case RIL_REQUEST_OEM_HOOK_RAW : return "OEM_HOOK_RAW";
    case RIL_REQUEST_OEM_HOOK_STRINGS : return "OEM_HOOK_STRINGS";
    case RIL_REQUEST_SCREEN_STATE : return "SCREEN_STATE";
    case RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION : return "SET_SUPP_SVC_NOTIFICATION";
    case RIL_REQUEST_WRITE_SMS_TO_SIM : return "WRITE_SMS_TO_SIM";
    case RIL_REQUEST_DELETE_SMS_ON_SIM : return "DELETE_SMS_ON_SIM";
    case RIL_REQUEST_SET_BAND_MODE : return "SET_BAND_MODE";
    case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE : return "QUERY_AVAILABLE_BAND_MODE";
    case RIL_REQUEST_STK_GET_PROFILE : return "STK_GET_PROFILE";
    case RIL_REQUEST_STK_SET_PROFILE : return "STK_SET_PROFILE";
    case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND : return "STK_SEND_ENVELOPE_COMMAND";
    case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE : return "STK_SEND_TERMINAL_RESPONSE";
    case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM : return "STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM";
    case RIL_REQUEST_EXPLICIT_CALL_TRANSFER : return "EXPLICIT_CALL_TRANSFER";
    case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE : return "SET_PREFERRED_NETWORK_TYPE";
    case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE : return "GET_PREFERRED_NETWORK_TYPE";
    case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS : return "GET_NEIGHBORING_CELL_IERY_PREFERRED_VOICE_PRIVACY_MODE";
    case RIL_REQUEST_CDMA_FLASH : return "CDMA_FLASH";
    case RIL_REQUEST_CDMA_BURST_DTMF : return "CDMA_BURST_DTMF";

    case RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY : return "CDMA_VALIDATE_AND_WRITE_AKEY";
    case RIL_REQUEST_CDMA_SEND_SMS : return "CDMA_SEND_SMS";
    case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE : return "CDMA_SMS_ACKNOWLEDGE";
    case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG : return "GSM_GET_BROADCAST_SMS_CONFIG";
    case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG : return "GSM_SET_BROADCAST_SMS_CONFIG";
    case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION : return "GSM_SMS_BROADCAST_ACTIVATION";
    case RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG : return "CDMA_GET_BROADCAST_SMS_CONFIG";
    case RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG : return "CDMA_SET_BROADCAST_SMS_CONFIG";
    case RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION : return "CDMA_SMS_BROADCAST_ACTIVATION";
    case RIL_REQUEST_CDMA_SUBSCRIPTION : return "CDMA_SUBSCRIPTION";
    case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM : return "CDMA_WRITE_SMS_TO_RUIM";
    case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM : return "CDMA_DELETE_SMS_ON_RUIM";
    case RIL_REQUEST_DEVICE_IDENTITY : return "DEVICE_IDENTITY";
    case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE : return "EXIT_EMERGENCY_CALLBACK_MODE";
    case RIL_REQUEST_GET_SMSC_ADDRESS : return "GET_SMSC_ADDRESS";
    case RIL_REQUEST_SET_SMSC_ADDRESS : return "SET_SMSC_ADDRESS";
    case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS : return "REPORT_SMS_MEMORY_STATUS";
    case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING : return "REPORT_STK_SERVICE_IS_RUNNING";
    case RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE : return "CDMA_GET_SUBSCRIPTION_SOURCE";
    case RIL_REQUEST_ISIM_AUTHENTICATION : return "ISIM_AUTHENTICATION";
    case RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU : return "ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU";
    case RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS : return "STK_SEND_ENVELOPE_WITH_STATUS";

    case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED : return "RADIO_STATE_CHANGED";
    case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED : return "CALL_STATE_CHANGED";
    case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED : return "VOICE_NETWORK_STATE_CHANGED";
    case RIL_UNSOL_RESPONSE_NEW_SMS : return "NEW_SMS";
    case RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT : return "NEW_SMS_STATUS_REPORT";
    case RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM : return "NEW_SMS_ON_SIM";
    case RIL_UNSOL_ON_USSD : return "UNSOL_ON_USSD";
    case RIL_UNSOL_ON_USSD_REQUEST : return "UNSOL_ON_USSD_REQUEST";
    case RIL_UNSOL_NITZ_TIME_RECEIVED : return "UNSOL_NITZ_TIME_RECEIVED";
    case RIL_UNSOL_SIGNAL_STRENGTH : return "UNSOL_SIGNAL_STRENGTH";
    case RIL_UNSOL_DATA_CALL_LIST_CHANGED : return "UNSOL_DATA_CALL_LIST_CHANGED";
    case RIL_UNSOL_SUPP_SVC_NOTIFICATION : return "UNSOL_SUPP_SVC_NOTIFICATION";
    case RIL_UNSOL_STK_SESSION_END : return "UNSOL_STK_SESSION_END";
    case RIL_UNSOL_STK_PROACTIVE_COMMAND : return "UNSOL_STK_PROACTIVE_COMMAND";
    case RIL_UNSOL_STK_EVENT_NOTIFY : return "UNSOL_STK_EVENT_NOTIFY";
    case RIL_UNSOL_STK_CALL_SETUP : return "UNSOL_STK_CALL_SETUP";
    case RIL_UNSOL_SIM_SMS_STORAGE_FULL : return "UNSOL_SIM_SMS_STORAGE_FULL";
    case RIL_UNSOL_SIM_REFRESH : return "UNSOL_SIM_REFRESH";
    case RIL_UNSOL_CALL_RING : return "UNSOL_CALL_RING";
    case RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED : return "SIM_STATUS_CHANGED";
    case RIL_UNSOL_RESPONSE_CDMA_NEW_SMS : return "CDMA_NEW_SMS";
    case RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS : return "NEW_BROADCAST_SMS";
    case RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL : return "UNSOL_CDMA_RUIM_SMS_STORAGE_FULL";
    case RIL_UNSOL_RESTRICTED_STATE_CHANGED : return "UNSOL_RESTRICTED_STATE_CHANGED";
    case RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE : return "UNSOL_ENTER_EMERGENCY_CALLBACK_MODE";
    case RIL_UNSOL_CDMA_CALL_WAITING : return "UNSOL_CDMA_CALL_WAITING";
    case RIL_UNSOL_CDMA_OTA_PROVISION_STATUS : return "UNSOL_CDMA_OTA_PROVISION_STATUS";
    case RIL_UNSOL_CDMA_INFO_REC : return "UNSOL_CDMA_INFO_REC";
    case RIL_UNSOL_OEM_HOOK_RAW : return "UNSOL_OEM_HOOK_RAW";
    case RIL_UNSOL_RINGBACK_TONE : return "UNSOL_RINGBACK_TONE";
    case RIL_UNSOL_RESEND_INCALL_MUTE : return "UNSOL_RESEND_INCALL_MUTE";
    case RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED : return "UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED";
    case RIL_UNSOL_CDMA_PRL_CHANGED : return "UNSOL_CDMA_PRL_CHANGED";
    case RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE : return "UNSOL_EXIT_EMERGENCY_CALLBACK_MODE";
    case RIL_UNSOL_RIL_CONNECTED : return "UNSOL_RIL_CONNECTED";
	
    default: return "<<UNKNOWN>>";
    }
}

// ---------------------------------------------------------------------------

class RILCall {
public:
    RILCall(const Parcel& data) {
	state              = static_cast<RIL_CallState>(data.readInt32());
	index              = data.readInt32();
	toa                = data.readInt32();
	isMpty             = (data.readInt32() != 0);
	isMT               = (data.readInt32() != 0);
	als                = data.readInt32();
	isVoice            = data.readInt32();
	isVoicePrivacy     = data.readInt32();
	number             = data.readString16();
	numberPresentation = data.readInt32();
	name               = data.readString16();
	namePresentation   = data.readInt32();
	hasUusInfo         = (data.readInt32() != 0);
	if (hasUusInfo) {
	    // ### TODO:  Check this with something that has UUS
	    uusType   = static_cast<RIL_UUS_Type>(data.readInt32());
	    uusDcs    = static_cast<RIL_UUS_DCS>(data.readInt32());
	    uusLength = data.readInt32();
	    uusData   = NULL;
	    if (uusLength > 0) {
		uusData = new char[uusLength];
		data.read(uusData, uusLength);
	    }
	}
    }

    ~RILCall() {
	if (hasUusInfo && uusData)
	    delete uusData;
    }

    RIL_CallState   state;
    int             index;      /* Connection Index for use with, eg, AT+CHLD */
    int             toa;        /* type of address, eg 145 = intl */
    char            isMpty;     /* nonzero if is mpty call */
    char            isMT;       /* nonzero if call is mobile terminated */
    char            als;        /* ALS line indicator if available
                                   (0 = line 1) */
    char            isVoice;    /* nonzero if this is is a voice call */
    char            isVoicePrivacy;     /* nonzero if CDMA voice privacy mode is active */
    String16        number;     /* Remote party number */
    int             numberPresentation; /* 0=Allowed, 1=Restricted, 2=Not Specified/Unknown 3=Payphone */
    String16        name;       /* Remote party name */
    int             namePresentation; /* 0=Allowed, 1=Restricted, 2=Not Specified/Unknown 3=Payphone */
    
    bool            hasUusInfo;
    RIL_UUS_Type    uusType;    /* UUS Type */
    RIL_UUS_DCS     uusDcs;     /* UUS Data Coding Scheme */
    int             uusLength;  /* Length of UUS Data */
    char *          uusData;    /* UUS Data */
};

// ---------------------------------------------------------------------------

class RILRequest {
public:
    // Use a static creator in case we decide later to keep a pool of request objects
    static RILRequest *create(int token, int message, const sp<IPhoneClient>& client) {
	return new RILRequest(client, token, message);
    }

    // Used internally by PhoneService
    static RILRequest *create(int message) { return new RILRequest(NULL, -1, message); }

    void writeString(const String16& data) { mParcel.writeString16(data); }
    void writeInt(int n) { mParcel.writeInt32(n); }
    void setError(int err) { mError = err; }

    const sp<IPhoneClient> client() const { return mClient; }
    const Parcel& parcel() const { return mParcel; }
    int           serial() const { return mSerialNumber; }
    int           token() const { return mToken; }
    int           message() const { return mMessage; }

private:
    RILRequest(const sp<IPhoneClient>& client, int token, int message) 
	: mClient(client)
	, mToken(token)
	, mMessage(message)
	, mSerialNumber(sSerialNumber++)
	, mError(0) {
	mParcel.writeInt32(message);
	mParcel.writeInt32(mSerialNumber);
    }

public:
    sp<IPhoneClient> mClient;
    Parcel           mParcel;
    int              mToken;
    int              mMessage;
    int              mSerialNumber;
    int              mError;

private:
    static int       sSerialNumber;
};

int RILRequest::sSerialNumber = 0;

// ---------------------------------------------------------------------------

class PhoneServerClient  
{
public:
    PhoneServerClient(const sp<android::IPhoneClient>& c, UnsolicitedMessages f)
	: client(c), flags(f) {}

    sp<android::IPhoneClient> client;
    UnsolicitedMessages       flags;
};

// ---------------------------------------------------------------------------


PhoneService::PhoneService()
    : mRILfd(-1)
    , mAudioMode(AUDIO_MODE_NORMAL)  // Strictly speaking we should initialize this 
{
    char *flags = ::getenv("DEBUG_PHONE");
    if (flags != NULL) {
	mDebug = DEBUG_BASIC;
	char *token = strtok(flags, ":");
	while (token != NULL) {
	    if (!strncasecmp(token, "in", 2))
		mDebug |= DEBUG_INCOMING;
	    else if (!strncasecmp(token, "out", 3))
		mDebug |= DEBUG_OUTGOING;
	    token = strtok(NULL, ":");
	}
    }

    mRILfd = socket_local_client("rild",
				 ANDROID_SOCKET_NAMESPACE_RESERVED,
				 SOCK_STREAM);
    if (mRILfd < 0) {
	perror("opening rild socket");
	exit(-1);
    }

    if (AudioSystem::setMasterMute(false) != NO_ERROR)
	LOG_ALWAYS_FATAL("Unable to write master mute to false\n");
    SLOGV("Audio configured\n");
    
    if (createThread(beginOutgoingThread, this) == false) 
	LOG_ALWAYS_FATAL("ERROR!  Unable to create outgoing thread for RILD socket\n");

    if (createThread(beginIncomingThread, this) == false) 
	LOG_ALWAYS_FATAL("ERROR!  Unable to create incoming thread for RILD socket\n");
}

PhoneService::~PhoneService()
{
    SLOGV("%s:%d  ~Phoneservice\n",__FILE__,__LINE__);
}

int PhoneService::beginIncomingThread(void *cookie)
{
    PhoneService *service = (PhoneService *)cookie;
    return service->incomingThread();
}

int PhoneService::beginOutgoingThread(void *cookie)
{
    PhoneService *service = (PhoneService *)cookie;
    return service->outgoingThread();
}

const int RESPONSE_SOLICITED = 0;
const int RESPONSE_UNSOLICITED = 1;

/*
  This thread reads from the RIL daemon and sends messages to 
  appropriate clients.

  ### TODO:  If we get a bad read from rild, we should drop this 
         server or log an error or do something suitable
 */

int PhoneService::incomingThread()
{
    while (1) {
	uint32_t header;
	int ret = read(mRILfd, &header, sizeof(header));

	if (ret != sizeof(header)) {
	    SLOGW("Read %d bytes instead of %d\n", ret, sizeof(header));
	    perror("PhoneService::incomingThread read on header");
	    return ret;
	}

	int data_size = ntohl(header);
	Parcel data;
	ret = read(mRILfd, data.writeInplace(data_size), data_size);
	if (ret != data_size) {
	    perror("PhoneService::incomingThread read on payload");
	    return ret;
	}

	if (mDebug & DEBUG_INCOMING) {
	    SLOGV("<<<<<<< INCOMING <<<<<<<<<<\n");
	    const uint8_t *ptr = data.data();
	    for (int i = 0 ; i < data_size ; i++ ) {
		SLOGV("%02x ", *ptr++);
		if ((i+1) % 8 == 0 || (i+1 >= data_size))
		    SLOGV("\n");
	    }
	    SLOGV("<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
	}

	data.setDataPosition(0);
	int type = data.readInt32();
//	SLOGV("New message received: %d bytes type=%s\n", data_size, 
//	       (type ==RESPONSE_SOLICITED ? "solicited" : "unsolicited"));

	if (type == RESPONSE_UNSOLICITED) 
	    receiveUnsolicited(data);
	else
	    receiveSolicited(data);
    }

    return NO_ERROR;
}

/*
  This thread writes messages to rild.

  #### TODO:  If we get a bad write to rild, we should drop this 
              server or log an error or do something suitable
 */

int PhoneService::outgoingThread()
{
    // Write messages to the socket here
    while (1) {
	// Grab the first item on the list
	mLock.lock();
	while (mOutgoingRequests.size() == 0)
	    mCondition.wait(mLock);

	RILRequest *request = mOutgoingRequests[0];
	mOutgoingRequests.removeAt(0);
	mPendingRequests.push(request);
	mLock.unlock();
	    
	const Parcel& p(request->parcel());
	// Send the item
	if (mDebug & DEBUG_OUTGOING) {
	    SLOGV(">>>>>>> OUTGOING >>>>>>>>>>\n");
	    const uint8_t *ptr = p.data();
	    for (size_t i = 0 ; i < p.dataSize() ; i++ ) {
		SLOGV("%02x ", *ptr++);
		if ((i+1) % 8 == 0 || (i+1 >= p.dataSize()))
		    SLOGV("\n");
	    }
	    SLOGV(">>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	}

	int header = htonl(p.dataSize());
	int ret = ::send(mRILfd, (const void *)&header, sizeof(header), 0);
	if (ret != sizeof(header)) {
	    perror("Socket write error when sending header"); 
	    return ret;
	}
	ret = ::send(mRILfd, p.data(), p.dataSize(), 0);
	if (ret != (int) p.dataSize()) {
	    perror("Socket write error when sending parcel"); 
	    return ret;
	}
    }
    return NO_ERROR;
}

void PhoneService::binderDied(const wp<IBinder>& who)
{
    SLOGV("binderDied\n");
    Mutex::Autolock _l(mLock);
    mClients.removeItem(who);
    // ### TODO: Remove all messages from the outgoing queue
}

/*
  Insert a message into the outgoing queue.  The outgoing thread
  will pick it up and write it to rild
 */

void PhoneService::sendToRILD(RILRequest *request)
{
    Mutex::Autolock _l(mLock);
    mOutgoingRequests.push(request);
    mCondition.signal();
}

RILRequest *PhoneService::getPending(int serial_number)
{
    Mutex::Autolock _l(mLock);
    for (size_t i = 0 ; i < mPendingRequests.size() ; i++) {
	RILRequest *request = mPendingRequests[i];
	if (request->serial() == serial_number) {
	    mPendingRequests.removeAt(i);
	    return request;
	}
    }
    return NULL;
}

/*
  An unsolicited message has been received from rild.  Broadcast
  the message to all registered clients.

  This function runs in the rild receiving thread.
 */

void PhoneService::receiveUnsolicited(const Parcel& data)
{
    int message = data.readInt32();
    int ivalue  = 0;
    String16 svalue;
    UnsolicitedMessages flags = UM_NONE;

    SLOGD("<<< Unsolicited message=%s [%d]\n", rilMessageStr(message), message);

    switch (message) {
    case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: 
	ivalue = data.readInt32(); 
	flags = UM_RADIO_STATE_CHANGED;
	SLOGD("    RIL Radio state changed to %d\n", ivalue);
	if (ivalue == RADIO_STATE_OFF) {
	    RILRequest * request = RILRequest::create(RIL_REQUEST_RADIO_POWER);
	    request->writeInt(1);  // One value
	    request->writeInt(1);  // Turn the radio on
	    sendToRILD(request);
	}
	break;

    case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:
	flags = UM_CALL_STATE_CHANGED;
	// Invoke RIL_REQUEST_GET_CURRENT_CALLS
	SLOGD("    Call state changed\n");
	sendToRILD(RILRequest::create(RIL_REQUEST_GET_CURRENT_CALLS));
	break;

    case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
	flags = UM_VOICE_NETWORK_STATE_CHANGED;
	SLOGD("    Voice network state changed\n");
	break;

    case RIL_UNSOL_NITZ_TIME_RECEIVED:
	svalue = data.readString16();
	flags = UM_NITZ_TIME_RECEIVED;
        break;

    case RIL_UNSOL_SIGNAL_STRENGTH:
	ivalue = data.readInt32();
	flags = UM_SIGNAL_STRENGTH;
	SLOGD("     Signal strength changed %d\n", ivalue);
	break;

    case RIL_UNSOL_RIL_CONNECTED: {
	int n = data.readInt32();  // Number of integers
	ivalue = data.readInt32();  // RIL Version
	SLOGD("    RIL connected version=%d\n", ivalue);
    } break;

    default:
	SLOGD("### Unhandled unsolicited message received from RIL: %d\n", message);
	break;
    }

    if (flags) {
	Mutex::Autolock _l(mLock);
	for (size_t i = 0 ; i < mClients.size() ; i++) {
	    PhoneServerClient *client = mClients.valueAt(i);
	    if (client->flags & flags)
		client->client->Unsolicited(message, ivalue, svalue);
	}
    }
}

void PhoneService::updateAudioMode(audio_mode_t mode)
{
    if (mode != mAudioMode) {
	SLOGV("######### Updating the audio mode from %d to %d #############\n", mAudioMode, mode);
	if (AudioSystem::setMode(mode) != NO_ERROR)
	    SLOGW("Unable to set the audio mode\n");
	mAudioMode = mode;
    }
}

/*
  A solicited message has been received from rild.  Remove the corresponding
  RILRequest from the pending queue and send a response to the appropriate
  client.
  
  This function runs in the rild receiving thread.
*/

void PhoneService::receiveSolicited(const Parcel& data)
{
    int serial = data.readInt32();
    int result = data.readInt32();

    RILRequest *request = getPending(serial);

    if (!request) {
	SLOGW("receiveSolicited: not requested serial=%d result=%d\n", serial, result);
	return;
    }

    SLOGV("<<< Solicited message=%s [%d] serial=%d result=%d\n", rilMessageStr(request->message()), 
	   request->message(), serial, result);

    int token   = request->token();
    int message = request->message();
    int ivalue  = 0;
    Parcel extra;

    switch (message) {
    case RIL_REQUEST_GET_SIM_STATUS: 
	ivalue = data.readInt32();   // Store the card state
	break;

    case RIL_REQUEST_GET_CURRENT_CALLS: {
	// We retrieve audio information for the client.
	// We also update the AudioFlinger audio state appropriately based
	//   on the current call state
	ivalue = data.readInt32();   // How many calls there are
	audio_mode_t audio_mode = AUDIO_MODE_NORMAL;
	for (int i = 0 ; i < ivalue ; i++) {
	    RILCall call(data);
	    CallState call_state(call.state, call.index, 
				 call.number, call.numberPresentation, 
				 call.name, call.namePresentation);
	    call_state.writeToParcel(&extra);
	    if (call.state == RIL_CALL_INCOMING)
		audio_mode = AUDIO_MODE_RINGTONE;
	    else if (call.state == RIL_CALL_ACTIVE || call.state == RIL_CALL_DIALING || call.state == RIL_CALL_ALERTING)
		audio_mode = AUDIO_MODE_IN_CALL;
	}
	
	SLOGV("    %d calls, audio_mode=%d\n", ivalue, audio_mode);
	updateAudioMode(audio_mode);
    }	break;

    case RIL_REQUEST_DIAL:
    case RIL_REQUEST_HANGUP:
    case RIL_REQUEST_ANSWER:
    case RIL_REQUEST_UDUB:
    case RIL_REQUEST_SET_MUTE:
	break;

    case RIL_REQUEST_GET_MUTE:
	ivalue = data.readInt32();
	break;

    case RIL_REQUEST_SIGNAL_STRENGTH:
	// In actuality, we should probably read all 12 signal strengths
	ivalue = data.readInt32();
	break;

    case RIL_REQUEST_VOICE_REGISTRATION_STATE:
	ivalue = data.readInt32();   // Starts with the number of strings
	for (int i = 0 ; i < ivalue ; i++)
	    extra.writeString16(data.readString16());
	break;
    
    case RIL_REQUEST_OPERATOR: {
	ivalue = data.readInt32();
	assert(ivalue == 3);
	extra.writeString16(data.readString16());
	extra.writeString16(data.readString16());
	extra.writeString16(data.readString16());
    } break;

    case RIL_REQUEST_RADIO_POWER:
	SLOGV("    RIL Radio Power\n");
	// No response....
	break;

    default:
	SLOGV("Unhandled RIL request %d\n", message);
	break;
    }

    if (request->client() != NULL) {
	SLOGV("    Passing solicited message to client token=%d message=%d result=%d ivalue=%d...\n",
	       token, message, result, ivalue);
	request->client()->Response( token, message, result, ivalue, extra );
    }
    delete request;
}

/*
  Lookup the PhoneServerClient.  If it doesn't exist, create a new empty one.
  You must hold the mLock before calling this method
 */

PhoneServerClient * PhoneService::getClient(const sp<IPhoneClient>& client)
{
    ssize_t index = mClients.indexOfKey(client->asBinder());
    if (index >= 0)
	return mClients.valueAt(index);

    SLOGV(".....creating new PhoneServerClient....\n");
    PhoneServerClient *phone_client = new PhoneServerClient(client,UM_NONE);
    mClients.add(client->asBinder(),phone_client);
    status_t err = client->asBinder()->linkToDeath(this, NULL, 0);
    if (err)
	SLOGW("PhoneService::Register linkToDeath failed %d\n", err);
    return phone_client;
}

// BnPhoneService methods

void PhoneService::Register(const sp<IPhoneClient>& client, UnsolicitedMessages flags)
{
    SLOGV("Client register flags=%0x\n", flags);

    Mutex::Autolock _l(mLock);
    getClient(client)->flags = flags;
}

void PhoneService::Request(const sp<IPhoneClient>& client, int token, int message, int ivalue, const String16& svalue) 
{
    SLOGV("Client request: token=%d message=%d ivalue=%d\n", token, message, ivalue);

    // Add the client if it doesn't exist
    {
	Mutex::Autolock _l(mLock);
	getClient(client);
    }

    RILRequest *request = RILRequest::create(token, message, client);
    switch(message) {
    case RIL_REQUEST_GET_SIM_STATUS:	break;
    case RIL_REQUEST_GET_CURRENT_CALLS:	break;
    case RIL_REQUEST_DIAL: 
	request->writeString(svalue);
	request->writeInt(0);   // CLIR: Use subscription default value
	request->writeInt(0);   // UUS information is absent
	request->writeInt(0);   // UUS information is absent
	break;
    case RIL_REQUEST_HANGUP:
	request->writeInt(1);
	request->writeInt(ivalue);   // This is really the GSM index
	break;

    case RIL_REQUEST_SIGNAL_STRENGTH:	       break;
    case RIL_REQUEST_VOICE_REGISTRATION_STATE: break;
    case RIL_REQUEST_OPERATOR:                 break;
    case RIL_REQUEST_ANSWER:                   break;
    case RIL_REQUEST_UDUB:                     break;

    case RIL_REQUEST_SET_MUTE:
	request->writeInt(1);
	request->writeInt(ivalue ? 1 : 0);
	break;

    case RIL_REQUEST_GET_MUTE:                 break;

    default:
	SLOGW("Unrecognized request %d\n", message);
	// ### TODO:  Put in an error code here and send the message back
	break;
    }

    sendToRILD(request);
}

// ---------------------------------------------------------------------------

status_t BnPhoneService::onTransact( uint32_t code, 
				    const Parcel& data, 
				    Parcel* reply, 
				    uint32_t flags )
{
    switch(code) {
    case REGISTER: {
	CHECK_INTERFACE(IPhoneSession, data, reply);
	sp<IPhoneClient> client = interface_cast<IPhoneClient>(data.readStrongBinder());
	int32_t myFlags = data.readInt32();
	Register(client,static_cast<UnsolicitedMessages>(myFlags));
	return NO_ERROR;
    } break;
    case REQUEST: {
	CHECK_INTERFACE(IPhoneSession, data, reply);
	sp<IPhoneClient> client = interface_cast<IPhoneClient>(data.readStrongBinder());
	int token   = data.readInt32();
	int message = data.readInt32();
	int ivalue  = data.readInt32();
	String16 svalue = data.readString16();
	Request(client, token, message, ivalue, svalue);
	return NO_ERROR;
    } break;
    }
    return BBinder::onTransact(code, data, reply, flags);
}

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(PhoneService)

};  // namespace android
