/*
  Common client interface for talking with Phone
*/

#ifndef _PHONE_CLIENT_H
#define _PHONE_CLIENT_H

#include <phone/IPhoneClient.h>
#include <phone/IPhoneService.h>

#include <utils/List.h>
#include <utils/Vector.h>
#include <binder/Parcel.h>
#include <telephony/ril.h>

namespace android {

class CallState {
public:
    RIL_CallState state;
    int           index;
    String16      number;
    String16      name;

public:
    CallState(const Parcel& parcel);   
    CallState(RIL_CallState inState, int inIndex, 
	      const String16& inNumber, int inNumberPresentation,
	      const String16& inName, int inNamePresentation);
    status_t writeToParcel(Parcel *parcel) const;
};


/**
 * Interface back to a client window
 *
 * Your client code should inherit from this interface and override the
 * functions that it wants to handle.
 */

class PhoneClient : public BnPhoneClient, public IBinder::DeathRecipient
{
public:
    // Request actions on the server
    void GetSIMStatus(int token);
    void GetCurrentCalls(int token);
    void Dial(int token, const String16& number);
    void Hangup(int token, int channel);
    void Reject(int token);
    void Operator(int token);
    void Answer(int token);
    void VoiceRegistrationState(int token);

    void Register( UnsolicitedMessages flags );

    size_t pending() const { return mPending.size(); }

    // BnPhoneClient
    virtual void Response(int token, int message, int result, int ivalue, const Parcel& extra);
    virtual void Unsolicited(int message, int ivalue, const String16& svalue);

protected:
    // Override these callbacks in your subclass to receive messages
    // The result is 0 if the operation succeeded, or an appropriate error code
    virtual void ResponseGetSIMStatus(int token, int result, bool simCardPresent) {}       //  1
    virtual void ResponseGetCurrentCalls(int token, int result, const List<CallState>&) {} //  9
    virtual void ResponseDial(int token, int result) {}                                    // 10
    virtual void ResponseHangup(int token, int result) {}                                  // 12
    virtual void ResponseReject(int token, int result) {}                                  // 17
    virtual void ResponseSignalStrength(int token, int result, int rssi) {}                // 19
    virtual void ResponseVoiceRegistrationState(int token, int result, 
						const List<String16>&) {}                  // 20
    virtual void ResponseOperator(int token, int result, const String16& longONS,
				  const String16& shortONS, const String16& code) {}       // 22
    virtual void ResponseAnswer(int token, int result) {}                                  // 40
    virtual void ResponseSetMute(int token, int result) {}                                 // 53
    virtual void ResponseGetMute(int token, int result, bool isMuteEnabled) {}             // 54

    // Unsolicited callbacks
    virtual void UnsolicitedRadioStateChanged(int state) {}           // 1000
    virtual void UnsolicitedCallStateChanged() {}                     // 1001
    virtual void UnsolicitedVoiceNetworkStateChanged() {}             // 1002
    virtual void UnsolicitedNITZTimeReceived(const String16& time) {} // 1008
    virtual void UnsolicitedSignalStrength(int rssi) {}               // 1009

private:
    virtual void onFirstRef();
    virtual void binderDied(const wp<IBinder>&);
    void         removeToken(int token);

private:
    sp<IPhoneService> mPhoneService;
    Vector<int>       mPending;
};

};

#endif  // _PHONE_CLIENT_H
