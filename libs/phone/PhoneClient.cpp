/*
  Common client libraries for talking with Phone
 */

#include <phone/PhoneClient.h>
#include <binder/BinderService.h>

namespace android {

CallState::CallState(const Parcel& parcel)
{
    state  = static_cast<RIL_CallState>(parcel.readInt32());
    index  = parcel.readInt32();
    number = parcel.readString16();
    name   = parcel.readString16();
}

static const char *presentationToString(int presentation)
{
    switch (presentation) {
    case 0:  return "Allowed";
    case 1:  return "Restricted";
    case 2:  return "Unknown";
    case 3:  return "Payphone";
    default: return "Unspecified";
    }
}

CallState::CallState(RIL_CallState inState, int inIndex, 
		     const String16& inNumber, int inNumberPresentation,
		     const String16& inName, int inNamePresentation)
    : state(inState)
    , index(inIndex)
{
    number = (inNumber.size() ? inNumber : String16(presentationToString(inNumberPresentation)));
    name = (inName.size() ? inName : String16(presentationToString(inNamePresentation)));
}

status_t CallState::writeToParcel(Parcel *parcel) const
{
    parcel->writeInt32(state);
    parcel->writeInt32(index);
    parcel->writeString16(number);
    parcel->writeString16(name);
    return NO_ERROR;
}

// ----------------------------------------------------------------------

void PhoneClient::GetSIMStatus(int token)
{
    mPending.push(token);
    mPhoneService->Request(this, token, RIL_REQUEST_GET_SIM_STATUS, 0, String16());
}

void PhoneClient::GetCurrentCalls(int token)
{
    mPending.push(token);
    mPhoneService->Request(this, token, RIL_REQUEST_GET_CURRENT_CALLS, 0, String16());
}

void PhoneClient::Dial(int token, const String16& number)
{
    mPending.push(token);
    mPhoneService->Request(this, token, RIL_REQUEST_DIAL, 0, number);
}

void PhoneClient::Hangup(int token, int channel)
{
    mPending.push(token);
    mPhoneService->Request(this, token, RIL_REQUEST_HANGUP, channel, String16());
}

void PhoneClient::Reject(int token)
{
    mPending.push(token);
    mPhoneService->Request(this, token, RIL_REQUEST_UDUB, 0, String16());
}

void PhoneClient::Operator(int token)
{
    mPending.push(token);
    mPhoneService->Request(this, token, RIL_REQUEST_OPERATOR, 0, String16());
}

void PhoneClient::Answer(int token)
{
    mPending.push(token);
    mPhoneService->Request(this, token, RIL_REQUEST_ANSWER, 0, String16());
}

void PhoneClient::VoiceRegistrationState(int token)
{
    mPending.push(token);
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#else
    mPhoneService->Request(this, token, RIL_REQUEST_VOICE_REGISTRATION_STATE, 0, String16());
#endif
}

void PhoneClient::Register(UnsolicitedMessages flags)
{
    mPhoneService->Register(this, flags);
}

void PhoneClient::onFirstRef()
{
    sp<IServiceManager> sm     = defaultServiceManager();
    sp<IBinder>         binder = sm->getService(String16("phone"));

    if (!binder.get()) {
	fprintf(stderr, "Unable to connect to phone service\n");
	exit(-1);
    }
    status_t err = binder->linkToDeath(this, 0);
    if (err) {
	fprintf(stderr, "Unable to link to phone death: %s", strerror(-err));
	exit(-1);
    }
  
    mPhoneService = interface_cast<IPhoneService>(binder);
}

void PhoneClient::binderDied(const wp<IBinder>& who)
{
    // printf("PhoneClient::binderDied\n");
    exit(-1);
}

void PhoneClient::removeToken(int token)
{
    for (size_t i=0 ; i < mPending.size() ; i++) {
	if (mPending[i] == token) {
	    mPending.removeAt(i);
	    return;
	}
    }
    fprintf(stderr, "PhoneClient: Received unrecognized token %d\n", token);
}

void PhoneClient::Response(int token, int message, int result, int ivalue, const Parcel& extra)
{
    // printf("PhoneClient::Response token=%d, message=%d, result=%d, ivalue=%d\n", token, message, result, ivalue);
    removeToken(token);

    switch (message) {
    case RIL_REQUEST_GET_SIM_STATUS:
	ResponseGetSIMStatus(token, result, ivalue);
	break;

    case RIL_REQUEST_GET_CURRENT_CALLS: {
	List<CallState> callList;
	for (int i = 0 ; i < ivalue ; i++)
	    callList.push_back(CallState(extra));
	ResponseGetCurrentCalls(token, result, callList);
    } break;

    case RIL_REQUEST_DIAL:
	ResponseDial(token, result);
	break;

    case RIL_REQUEST_HANGUP:
	ResponseHangup(token, result);
	break;

    case RIL_REQUEST_UDUB:
	ResponseReject(token, result);
	break;

    case RIL_REQUEST_SIGNAL_STRENGTH:
	ResponseSignalStrength(token, result, ivalue);
	break;

#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#else
    case RIL_REQUEST_VOICE_REGISTRATION_STATE: {
	List<String16> strlist;
	for (int i = 0 ; i < ivalue ; i++)
	    strlist.push_back(extra.readString16());
	ResponseVoiceRegistrationState(token, result, strlist);
    } break;
#endif

    case RIL_REQUEST_OPERATOR: {
	String16 longONS  = extra.readString16();
	String16 shortONS = extra.readString16();
	String16 code     = extra.readString16();
	ResponseOperator(token, result, longONS, shortONS, code);
    } break;

    case RIL_REQUEST_ANSWER:
	ResponseAnswer(token, result);
	break;

    case RIL_REQUEST_SET_MUTE:
	ResponseSetMute(token, result);
	break;

    case RIL_REQUEST_GET_MUTE:
	ResponseGetMute(token, result, ivalue);
	break;
    }
}

void PhoneClient::Unsolicited(int message, int ivalue, const String16& svalue)
{
    switch (message) {
    case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED:
	UnsolicitedRadioStateChanged(ivalue);
	break;

    case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:
	UnsolicitedCallStateChanged();
	break;

#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#else
    case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
	UnsolicitedVoiceNetworkStateChanged();
	break;
#endif

    case RIL_UNSOL_NITZ_TIME_RECEIVED:
	UnsolicitedNITZTimeReceived(svalue);
	break;

    case RIL_UNSOL_SIGNAL_STRENGTH:
	UnsolicitedSignalStrength(ivalue);
	break;

    default:
	// printf("PhoneClient::Unsolicited message %d ivalue=%d\n", message, ivalue);
	break;
    }
}

// ----------------------------------------------------------------------

}; // namespace Android
