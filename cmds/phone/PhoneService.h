/*
  The PhoneService is the binder object exposed by Phone.  
 */

#ifndef _PHONE_SERVICE_H
#define _PHONE_SERVICE_H

#include <binder/BinderService.h>
#include <phone/IPhoneService.h>
#include <utils/Singleton.h>
#include <utils/List.h>
#include <media/AudioSystem.h>

namespace android {

class PhoneServerClient;
class RILRequest;

class PhoneService : public BinderService<PhoneService>,
	public BnPhoneService,
	public IBinder::DeathRecipient,
	public Singleton<PhoneService>
{
    friend class BinderService<PhoneService>;

public:
    // for PhoneService
    static const char *getServiceName() { return "phone"; }

    PhoneService();
    virtual ~PhoneService();

    // IBinder::DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);

    // BnPhoneService
    void Register(const sp<IPhoneClient>& client, UnsolicitedMessages flags);
    void Request(const sp<IPhoneClient>& client, int token, int message, int ivalue, const String16& svalue);

private:
    static int  beginIncomingThread(void *cookie);
    static int  beginOutgoingThread(void *cookie);

    int         incomingThread();
    int         outgoingThread();

    PhoneServerClient *getClient(const sp<IPhoneClient>& client);
    
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

    KeyedVector< wp<IBinder>, PhoneServerClient *> mClients;

    Vector<RILRequest *> mOutgoingRequests;
    Vector<RILRequest *> mPendingRequests;
 };

// ---------------------------------------------------------------------------

}; // namespace android


#endif // _PHONE_SERVICE_H

