/*
  The SigynService is the binder object exposed by Sigyn.  
 */

#ifndef _SIGYN_SERVICE_H
#define _SIGYN_SERVICE_H

#include <binder/BinderService.h>
#include <sigyn/ISigynService.h>
#include <utils/Singleton.h>
#include <utils/List.h>
#include <media/AudioSystem.h>

namespace android {

class SigynServerClient;
class RILRequest;

class SigynService : public BinderService<SigynService>,
	public BnSigynService,
	public IBinder::DeathRecipient,
	public Singleton<SigynService>
{
    friend class BinderService<SigynService>;

public:
    // for SigynService
    static const char *getServiceName() { return "phone"; }

    SigynService();
    virtual ~SigynService();

    // IBinder::DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);

    // BnSigynService
    void Register(const sp<ISigynClient>& client, UnsolicitedMessages flags);
    void Request(const sp<ISigynClient>& client, int token, int message, int ivalue, const String16& svalue);

private:
    static int  beginIncomingThread(void *cookie);
    static int  beginOutgoingThread(void *cookie);

    int         incomingThread();
    int         outgoingThread();

    SigynServerClient *getClient(const sp<ISigynClient>& client);
    
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

    KeyedVector< wp<IBinder>, SigynServerClient *> mClients;

    Vector<RILRequest *> mOutgoingRequests;
    Vector<RILRequest *> mPendingRequests;
 };

// ---------------------------------------------------------------------------

}; // namespace android


#endif // _SIGYN_SERVICE_H

