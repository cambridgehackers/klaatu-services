/*
   Keep a list of scanned stations
 */

#ifndef _SCANNED_STATIONS_H
#define _SCANNED_STATIONS_H

#include <wifi/IWifiClient.h>
#include <binder/Parcel.h>
#include <utils/String8.h>

namespace android {

class ScannedStations {
public:
    void update(const String8&);
    const Vector<ScannedStation>& data() const { return mStations; }

private:
    // TODO:  Probably need a Mutex lock
    Vector<ScannedStation>  mStations;
};

};  // namespace android

#endif // _SCANNED_STATIONS_H
