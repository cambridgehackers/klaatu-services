/*
  The PhoneService is the binder object exposed by Phone.  
 */

#ifndef _PHONE_SERVICE_H
#define _PHONE_SERVICE_H

#include <binder/BinderService.h>
#include <phone/IPhoneService.h>
#include <utils/List.h>
#include <media/AudioSystem.h>

namespace android {

class PhoneServerClient;
class PhoneMachine;

class PhoneService : public BinderService<PhoneService>,
	public BnPhoneService, public IBinder::DeathRecipient
{
    friend class BinderService<PhoneService>;

public:
    // for PhoneService
    PhoneService();
    virtual ~PhoneService();
    static const char *getServiceName() { return "phone"; }

    // IBinder::DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);

    // BnPhoneService
    void Register(const sp<IPhoneClient>& client, UnsolicitedMessages flags);
    void Request(const sp<IPhoneClient>& client, int token, int message, int ivalue, const String16& svalue);

    // Methods called from service
    void broadcastUnsolicited(UnsolicitedMessages flags, int message, int ivalue, String16 svalue);

private:
    PhoneServerClient *getClient(const sp<IPhoneClient>& client);
    PhoneMachine      *mMachine;
    mutable Mutex     mLock;
    KeyedVector< wp<IBinder>, PhoneServerClient *> mClients;
};

}; // namespace android

#endif // _PHONE_SERVICE_H
