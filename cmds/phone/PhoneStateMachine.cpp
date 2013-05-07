/*
  PhoneService
*/

#define LOG_NDEBUG 0
#define LOG_TAG "KlaatuPhone"

#include "PhoneService.h"
#include "PhoneStateMachine.h"
#include <phone/IPhoneClient.h>
#include <phone/PhoneClient.h>
#include <utils/threads.h>
#include <cutils/sockets.h>
#include <arpa/inet.h>
#include <telephony/ril.h>
#include <cutils/properties.h>

namespace android {

static const char * rilMessageStr(int message)
{
    switch (message) {
#include "rildebugstr.generated.h"
    default: return "<<UNKNOWN>>";
    }
}

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
    char            als;        /* ALS line indicator if available (0 = line 1) */
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
    RILRequest(const sp<IPhoneClient>& client, int token, int message) 
	: mClient(client) , mToken(token) , mMessage(message)
	, mSerialNumber(sSerialNumber++) , mError(0) {
	mParcel.writeInt32(message);
	mParcel.writeInt32(mSerialNumber);
    }
    void writeString(const String16& data) { mParcel.writeString16(data); }
    void writeInt(int n) { mParcel.writeInt32(n); }
    void setError(int err) { mError = err; }
    const sp<IPhoneClient> client() const { return mClient; }
    const Parcel& parcel() const { return mParcel; }
    int           serial() const { return mSerialNumber; }
    int           token() const { return mToken; }
    int           message() const { return mMessage; }
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

void PhoneMachine::updateAudioMode(audio_mode_t mode)
{
    if (mode != mAudioMode) {
	SLOGV("######### Updating the audio mode from %d to %d #############\n", mAudioMode, mode);
	if (AudioSystem::setMode(mode) != NO_ERROR)
	    SLOGW("Unable to set the audio mode\n");
	mAudioMode = mode;
    }
}

RILRequest *PhoneMachine::getPending(int serial_number)
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
  This thread writes messages to rild socket.
  #### TODO:  If we get a bad write to rild, we should drop this 
              server or log an error or do something suitable
 */

int PhoneMachine::outgoingThread()
{
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

/*
  Insert a message into the outgoing queue.  The outgoing thread
  will pick it up and write it to rild
 */

void PhoneMachine::sendToRILD(RILRequest *request)
{
    Mutex::Autolock _l(mLock);
    mOutgoingRequests.push(request);
    mCondition.signal();
}

/*
  An unsolicited message has been received from rild.  Broadcast
  the message to all registered clients.
 */
typedef struct {
    int message;
    UnsolicitedMessages flags;
} MAPTYPE;
static MAPTYPE eventmap[] = {
    {RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, UM_RADIO_STATE_CHANGED},
    {RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, UM_CALL_STATE_CHANGED},
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#else
    {RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, UM_VOICE_NETWORK_STATE_CHANGED},
#endif
    {RIL_UNSOL_NITZ_TIME_RECEIVED, UM_NITZ_TIME_RECEIVED},
    {RIL_UNSOL_SIGNAL_STRENGTH, UM_SIGNAL_STRENGTH}, {0,UM_NONE}};

void PhoneMachine::receiveUnsolicited(const Parcel& data)
{
    int message = data.readInt32();
    int ivalue  = 0;
    String16 svalue;
    UnsolicitedMessages flags = UM_NONE;
    MAPTYPE *pmap = eventmap;

    SLOGD("<<< Unsolicited message=%s [%d]\n", rilMessageStr(message), message);
    while (pmap->flags != UM_NONE && message != pmap->message)
        pmap++;
    if (pmap->flags)
	flags = pmap->flags;
    switch (message) {
    case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: 
	ivalue = data.readInt32(); 
	SLOGD("    RIL Radio state changed to %d\n", ivalue);
	if (ivalue == RADIO_STATE_OFF) {
	    RILRequest * request = new RILRequest(NULL, -1, RIL_REQUEST_RADIO_POWER);
	    request->writeInt(1);  // One value
	    request->writeInt(1);  // Turn the radio on
	    sendToRILD(request);
	}
	break;
    case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:
	sendToRILD(new RILRequest(NULL, -1, RIL_REQUEST_GET_CURRENT_CALLS));
	break;
    case RIL_UNSOL_NITZ_TIME_RECEIVED:
	svalue = data.readString16();
        break;
    case RIL_UNSOL_SIGNAL_STRENGTH:
	ivalue = data.readInt32();
	SLOGD("     Signal strength changed %d\n", ivalue);
	break;
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#else
    case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
	break;
    case RIL_UNSOL_RIL_CONNECTED: {
	int n = data.readInt32();  // Number of integers
	ivalue = data.readInt32();  // RIL Version
	SLOGD("    RIL connected version=%d\n", ivalue);
        break;
        }
#endif
    default:
	SLOGD("### Unhandled unsolicited message received from RIL: %d\n", message);
	break;
    }
    if (flags)
	mService->broadcastUnsolicited(flags, message, ivalue, svalue);
}

/*
  A solicited message has been received from rild.  Remove the corresponding
  RILRequest from the pending queue and send a response to the appropriate
  client.
*/

void PhoneMachine::receiveSolicited(const Parcel& data)
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
        break;
        }
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
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#else
    case RIL_REQUEST_VOICE_REGISTRATION_STATE:
	ivalue = data.readInt32();   // Starts with the number of strings
	for (int i = 0 ; i < ivalue ; i++)
	    extra.writeString16(data.readString16());
	break;
#endif
    case RIL_REQUEST_OPERATOR: {
	ivalue = data.readInt32();
	assert(ivalue == 3);
	extra.writeString16(data.readString16());
	extra.writeString16(data.readString16());
	extra.writeString16(data.readString16());
        break;
        }
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

// ---------------------------------------------------------------------------

const int RESPONSE_SOLICITED = 0;

/*
  This thread reads from the RIL daemon and sends messages to 
  appropriate clients.
  ### TODO:  If we get a bad read from rild, we should drop this 
         server or log an error or do something suitable
 */

int PhoneMachine::incomingThread()
{
	char boardname[PROPERTY_VALUE_MAX];
	if (property_get("ro.product.board", boardname, "") > 0) {
		SLOGV("%s: -------------Board %s -----------------\n", __FUNCTION__, boardname);
		// QCOM Version 4.1.2 onwards, supports multiple clients and needs a SUB1/2 string to
		// identify client
		// For this example code we will just use "SUB1" for client 0
		if ( !strcasecmp(boardname, "msm8960")) {
			char *sub = "SUB1";
			int ret = ::send(mRILfd, sub, sizeof(sub), 0);
			if (ret != (int) sizeof(sub)) {
	    		perror("Socket write error when sending parcel"); 
	    		return ret;
			}
			SLOGV("%s: sent SUB1\n", __FUNCTION__);
		}
	}
	else SLOGE("%s: could not get device name\n", __FUNCTION__);

    while (1) {
	uint32_t header;
	int ret = read(mRILfd, &header, sizeof(header));

	if (ret != sizeof(header)) {
	    SLOGW("Read %d bytes instead of %d\n", ret, sizeof(header));
	    perror("PhoneMachine::incomingThread read on header");
	    return ret;
	}
	int data_size = ntohl(header);
	Parcel data;
	ret = read(mRILfd, data.writeInplace(data_size), data_size);
	if (ret != data_size) {
	    perror("PhoneMachine::incomingThread read on payload");
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
	if (type == RESPONSE_SOLICITED) 
	    receiveSolicited(data);
	else
	    receiveUnsolicited(data);
    }
    return NO_ERROR;
}

void PhoneMachine::Request(const sp<IPhoneClient>& client, int token, int message, int ivalue, const String16& svalue) 
{
    SLOGV("Client request: token=%d message=%d ivalue=%d\n", token, message, ivalue);

    RILRequest *request = new RILRequest(client, token, message);
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
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#else
    case RIL_REQUEST_VOICE_REGISTRATION_STATE: break;
#endif
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

static int beginIncomingThread(void *cookie)
{
    return static_cast<PhoneMachine *>(cookie)->incomingThread();
}

static int beginOutgoingThread(void *cookie)
{
    return static_cast<PhoneMachine *>(cookie)->outgoingThread();
}

PhoneMachine::PhoneMachine(PhoneService *service)
    : mRILfd(-1)
    , mAudioMode(AUDIO_MODE_NORMAL)  // Strictly speaking we should initialize this 
    , mService(service)
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
    mRILfd = socket_local_client("rild", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
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

};  // namespace android
