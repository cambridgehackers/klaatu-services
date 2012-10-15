/*
  ISigynService
*/

#ifndef _ISIGYN_SERVICE_H
#define _ISIGYN_SERVICE_H

#include <binder/IInterface.h>
#include <sigyn/ISigynClient.h>

namespace android {

// ----------------------------------------------------------------------------

/**
 * Each application should make a single ISigynService connection 
 */
    
class ISigynService : public IInterface
{
protected:
    enum {
	REGISTER = IBinder::FIRST_CALL_TRANSACTION,
	REQUEST
    };

public:
    DECLARE_META_INTERFACE(SigynService);

    virtual void Register(const sp<ISigynClient>& client, UnsolicitedMessages flags) = 0;
    virtual void Request(const sp<ISigynClient>& client, int token, int message, int ivalue, const String16& svalue) = 0;
};

// ----------------------------------------------------------------------------

class BnSigynService : public BnInterface<ISigynService>
{
public:
    virtual status_t onTransact(uint32_t code, 
				const Parcel& data,
				Parcel* reply, 
				uint32_t flags = 0);
};

// ----------------------------------------------------------------------------

}; // namespace android

#endif // _ISIGYN_SERVICE_H

