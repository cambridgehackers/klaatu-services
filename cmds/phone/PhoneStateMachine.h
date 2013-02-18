/*
  The PhoneMachine is the binder object exposed by Phone.  
 */

#ifndef _PHONE_MACHINE_H
#define _PHONE_MACHINE_H

#include <utils/List.h>
#include <media/AudioSystem.h>
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
typedef audio_mode_e audio_mode_t;
#endif

namespace android {
class RILRequest;
class PhoneService;

class PhoneMachine
{
public:
    PhoneMachine(PhoneService *service);
    ~PhoneMachine() {};
    int         incomingThread();
    int         outgoingThread();

    // called from Service
    void Request(const sp<IPhoneClient>& client, int token, int message, int ivalue, const String16& svalue);

private:
    void        sendToRILD(RILRequest *request);
    RILRequest *getPending(int serial_number);
    void        receiveUnsolicited(const Parcel& data);
    void        receiveSolicited(const Parcel& data);
    void        updateAudioMode(audio_mode_t mode);

private:
    enum DebugFlags { DEBUG_NONE=0, 
		      DEBUG_BASIC=0x1,
		      DEBUG_INCOMING=0x2, 
		      DEBUG_OUTGOING=0x4 };
    mutable Mutex     mLock;
    mutable Condition mCondition;
    int               mRILfd;
    audio_mode_t      mAudioMode;
    int               mDebug;
    Vector<RILRequest *> mOutgoingRequests;
    Vector<RILRequest *> mPendingRequests;
    PhoneService     *mService;
 };

}; // namespace android

#endif // _PHONE_MACHINE_H
