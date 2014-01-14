/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/sensor.h>

#include <SfwSensor.h>
#include <SfwSensorManager.h>
#include <SfwMessage.h>
#include <SfwParameters.h>
#include <sp_sensors.h>
#include <activity_types.h>

#include <utils/Looper.h>
#include <cstdio>
#include <cstdarg>
#include <sensors/sensor.h>

using namespace android;
using namespace sfw;

int onSensorChanged(ASensorEvent buffer);

SfwSensorManager mgr;
const SfwSensor *sensor = 0;
sp<Looper> mloop;
void *mCookie=NULL;
// For printing multiline event data
#define NL_PAD "\n\t\t\t\t"

nsecs_t startedAt = 0;
int nsensors = 0;
bool showTimes = false;
bool keepRunning = true;

static FILE *csvFile = 0;
#define CSVPRINT if (!csvFile) {} else csvPrintf

static void csvPrintf(const char *format, ...) {
    va_list args;

    va_start(args, format);
    if (csvFile)
        vfprintf(csvFile, format, args);
    va_end(args);
}

nsecs_t tsCheck(const char *msg = 0)
{
    static int64_t lastCheckpointTs = systemTime();

    int64_t ts = systemTime();
    int64_t d = ts - lastCheckpointTs;
    lastCheckpointTs = ts;

    // !showTimes can be overriden by NULL argument
    if (!showTimes && msg)
        return ts;

    //printf("[%lld] +%6lld us| %s", ts / 1000, d / 1000, msg ? msg : "");
    return ts;
}

class EvHandler: public SfwEventHandler
{
public:
    EvHandler()
      : mStartTime(systemTime())
      , mPrevTs(mStartTime)
      , mPrevRangeTs()
      , mPrevRangeTs0()
      , mPrevRangeCount()
      , mPrevRangeCount0()
      , mEvCount()
      , mExit(false)
    {
        tsCheck("START handler\n");
    }

    void sensorEvent(const SfwMessage &event);
    void sfwEvent(SfwSignal sig);
    void sensorSignal(const SfwSensor *sensor, const SfwParameters &details);

    nsecs_t mStartTime;
    nsecs_t mPrevTs;
    nsecs_t mPrevRangeTs; //< for estimation of actual rate: last and "all" values
    nsecs_t mPrevRangeTs0;
    int mPrevRangeCount;
    int mPrevRangeCount0;
    int mEvCount;
    bool mExit;
};

// event-handler function; it is called for us by a SfwSensorManager as a result of polling
void EvHandler::sensorEvent(const SfwMessage &event)
{
    nsecs_t now = tsCheck(0); // NOTE: NULL arg forces printout of timestamp

    const SensorEvent &ev = *event.event();
    nsecs_t deltaMs = int64_t(ev.ts - mPrevTs) / 1000000;

    mPrevTs = ev.ts;

    ++mEvCount;
    if (deltaMs < 0)
        deltaMs = 0; // for the first event

    //printf("%4u# %8lld us/%3lld ms (dtyp %d)", mEvCount, ev.ts / 1000, deltaMs, ev.typeTag);

    const float* data = &ev.get<float>(0);
    switch(ev.typeTag) {
        case DT_XYZ:
        case DT_ANGULARRATE:
        case DT_ORIENTATION:
            //printf("\t%9.6f %9.6f %9.6f\n", data[0], data[1], data[2]);
            //CSVPRINT("%d,%lld,%f,%f,%f\n", ev.typeTag, ev.ts, data[0], data[1], data[2]);
		#if 1
		ASensorEvent buffer;
		int rotation ;
			buffer.data[0] = data[0];
			buffer.data[1] = data[1];
			buffer.data[2] = data[2];
			buffer.timestamp = ev.ts;
		//printf("buffer.timestamp  = %llu, ev.ts = %llu (%llx)\n", buffer.timestamp, ev.ts, ev.ts);
		rotation = onSensorChanged(buffer);
                if (rotation >= 0) {
			int nr = rotation;
			if (rotation == 1)
				nr = 3;
			else if (rotation == 3)
				nr = 1;

		printf("rotation=%d, corrected rotation = %d \n", rotation, nr);
                if (static_cast<KlaatuSensor *>(mCookie)->sensor_event_handler)
                        static_cast<KlaatuSensor *>(mCookie)->sensor_event_handler(Sensor::TYPE_ACCELEROMETER, nr);
                }
		#endif
        break;

        case DT_REAL:
            printf("\tvalue=%8f\n", data[0]);
            CSVPRINT("%d,%lld,%f\n", ev.typeTag, ev.ts, data[0]);
            break;

        case DT_ACTREC_LVL_0: {
            int32_t activity = ev.get<int32_t>(0);
            const char *label = sfw::activityLabel(activity);
            printf("\tactivity_id=%d %s\n", activity, label);
            CSVPRINT("%d,%lld,%d,%s\n", ev.typeTag, ev.ts, activity, label);
            break;
        }

        case DT_MOTION: {
            const SensorsMotionData &m = ev.get<SensorsMotionData>();
            const Vec3 &acc = m.acceleration;
            const Vec3 &accG = m.accelerationIncludingGravity;
            const Vec3 &rot = m.rotationRate;
            printf(NL_PAD "Acc(%9.6f %9.6f %9.6f)"
                   NL_PAD "A+G(%9.6f %9.6f %9.6f)"
                   NL_PAD "Rot(%9.6f %9.6f %9.6f)\n",
                    acc.x, acc.y, acc.z, accG.x, accG.y, accG.z, rot.x, rot.y, rot.z);
            CSVPRINT("%d,%lld,%f,%f,%f\n", ev.typeTag, ev.ts, data[0]);
            break;
        }

        case DT_QUATERNION_Q30: {
            const sfw::QuaternionData &q = ev.get<sfw::Quaternion_Q30>();
            printf("\tQ(%9.6f | %9.6f %9.6f %9.6f)\n",
                    q30_to_double(q.w), q30_to_double(q.x), q30_to_double(q.y), q30_to_double(q.z));
            CSVPRINT("%d,%lld,%f,%f,%f\n", ev.typeTag, ev.ts,
                    q30_to_double(q.w), q30_to_double(q.x), q30_to_double(q.y), q30_to_double(q.z));
            break;
        }

        case DT_SENSORFUSION_Q: {
            const sfw::SensorFusion_Q &f = ev.get<sfw::SensorFusion_Q>();
            printf(NL_PAD "A(%f, %f, %f)" NL_PAD "M(%f, %f, %f)"
                   NL_PAD "G(%f, %f, %f)" NL_PAD "Q(%f | %f, %f, %f)\n",
                    q16_to_double(f.accel.x),   q16_to_double(f.accel.y), q16_to_double(f.accel.z),
                    q16_to_double(f.mag.x),     q16_to_double(f.mag.y),   q16_to_double(f.mag.z),
                    q16_to_double(f.gyro.x),    q16_to_double(f.gyro.y),  q16_to_double(f.gyro.z),

                    q30_to_double(f.quaternion.w),
                    q30_to_double(f.quaternion.x),
                    q30_to_double(f.quaternion.y),
                    q30_to_double(f.quaternion.z) );

            CSVPRINT("%d,%lld,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n", ev.typeTag, ev.ts,
                    q16_to_double(f.accel.x),   q16_to_double(f.accel.y), q16_to_double(f.accel.z),
                    q16_to_double(f.mag.x),     q16_to_double(f.mag.y),   q16_to_double(f.mag.z),
                    q16_to_double(f.gyro.x),    q16_to_double(f.gyro.y),  q16_to_double(f.gyro.z),

                    q30_to_double(f.quaternion.w),
                    q30_to_double(f.quaternion.x),
                    q30_to_double(f.quaternion.y),
                    q30_to_double(f.quaternion.z) );
            break;
        }

        case DT_GPS: {
            GpsData *loc = (GpsData *)ev.payload();
            printf("\t%6f\t%6f\n", loc->latitude, loc->longitude);
            CSVPRINT("%d,%lld,%f,%f\n", ev.typeTag, ev.ts, loc->latitude, loc->longitude);
            break;
        }

        case DT_LOCANCHOR: {
            StayRegionData *loc = (StayRegionData *)ev.payload();
            printf("\t%6f\t%6f\n", loc->latitude, loc->longitude);
            CSVPRINT("%d,%lld,%f,%f\n", ev.typeTag, ev.ts, loc->latitude, loc->longitude);
            break;
        }

        default:
            printf("\t{values not shown} unknown sensor type=%d\n", ev.typeTag);
            break;
    }

    if (!showTimes) {
        // skip ELSE's
    } else if (mEvCount == 1) {
        printf("FIRST EVENT in %lld ms after enable()\n\n", (now - startedAt) / 1000000);
        mPrevRangeTs0 = mPrevRangeTs = now;
    } else if (nsensors == 1 && (mEvCount % 100) == 0) {
        printf("\n### RATE %f Hz (%.2f ms) -- global %f Hz (%.2f ms)\n\n",
               // Last  block
               (mEvCount - mPrevRangeCount) * 1000000000ULL / double(now - mPrevRangeTs),
               (now - mPrevRangeTs) / 1000000.0 / (mEvCount - mPrevRangeCount),
               // All samples
               mEvCount * 1000000000ULL / double(now - mPrevRangeTs0),
               (now - mPrevRangeTs0) / 1000000.0 / mEvCount
               );

        mPrevRangeTs = now;
        mPrevRangeCount = mEvCount;
    }
}

void EvHandler::sfwEvent(SfwEventHandler::SfwSignal sig)
{
    if(sig == SfwEventHandler::SfwSignalDisconnected) {
        printf("SFW service disconnected\n");
        mExit = true;
    }
}

void EvHandler::sensorSignal(const SfwSensor *sensor, const SfwParameters &details)
{
    printf("SENSOR SIGNAL: #%d %s (type %d)\n==> %s",
           sensor->handle(), sensor->name().string(), sensor->sensorType(),
           details.toString().string());
}

// Maps names to sensor type. The array is null terminated.
struct sensor_desc {
    const char *altName;
    const char *name;
    int getByName; // get sensor with SFW API by using name, not as a default sensor of a type
    sfw::SensorType type;
};

const sensor_desc sensorTypeMap[] = {
    {"acceleration","accelerometer", 0, sfw::SensorTypeAccelerometer},
    {"light",       "als",          0, sfw::SensorTypeLight},
    {"prx",         "proximity",    0, sfw::SensorTypeProximity},
    {"temperature", "ambienttempr", 0, sfw::SensorTypeAmbientTemperature},
    {"w3corient",   "orientation",  0, sfw::SensorTypeOrientation},
    {"w3cmotion",   "motion",       0, sfw::SensorTypeMotion},
    {"",            "quaternion",   0, sfw::SensorTypeOrientationQuaternion},
    {"fusion",      "motion-fusion", 0, sfw::SensorTypeMotionSensorFusion},
    {"gps",         "geolocation",  0, sfw::SensorTypeGPS},
    {"",            "gyroscope",    0, sfw::SensorTypeGyroscope},
    {"",            "magnetometer", 0, sfw::SensorTypeMagneticField},
    {"heading",     "compass",      0, sfw::SensorTypeCompass},
    {"",            "activity",     0, sfw::SensorTypeActivity},
    {"linearaccel", "linacc",       0, sfw::SensorTypeLinearAcceleration},
    {"",            "gravity",      0, sfw::SensorTypeGravity},
    {"locanchor",   "LocationAnchor", 0, sfw::SensorTypeLocAnchor},
    {"rotationvector", "rotvec",    0, sfw::SensorTypeRotationVector},
    {"pressure",    "barometer",    0, sfw::SensorTypePressure},
    {"relativehumidity", "humidity", 0, sfw::SensorTypeRelativeHumidity},
    {"shakeclassify", "activity.shakeclassify", 1, sfw::SensorTypeActivity},
    {"skolar",      "activity.skolar", 1, sfw::SensorTypeActivity},
    {"",            "tapdetector",  1, sfw::SensorTypeTapDetector},
    {0, 0, 0, sfw::SensorType_undefined}  // null terminated list
};

// Get sensor type from argument sensor name
const sensor_desc *selectSensorByName(const char *arg, bool byNameOnly = false)
{
     // first sensor with 'arg' matching as prefix to the short or SFW name is taken
    size_t len = strlen(arg);
    const sensor_desc *sensor = sensorTypeMap;
    for (; sensor->type != sfw::SensorType_undefined; ++sensor) {
        if (!byNameOnly && sensor->altName[0] && !strncmp(arg, sensor->altName, len))
            break;
        if (!strncmp(arg, sensor->name, len))
            break;
    }

    return sensor->name ? sensor : 0;
}

void printSensorInfo(const SfwSensor &s, bool brief = true)
{
    // Note: SFW provides only common names, NULL can be returned for others
    const char *typeName = sfw::defaultName(sfw::SensorType(s.sensorType())).c_str();

    printf("\tName: %s:\n"
           "\tDescription: %s:\n"
           "\tVendor: %s\n"
           "\tType: %d (%s)\n",
            s.name().string(),
            s.description().string(),
            s.vendor().string(),
            s.sensorType(), (typeName ? typeName : "?noname?"));

    if (!brief) {
        printf("\tVersion: %d\n"
               "\tInterval (min): %d\n"
               "\tRange: [%f, %f]\n"
               "\tResolution: %f\n"
               "\tPower: %f\n",
               s.version(),
               s.interval(),
               s.minValue(), s.maxValue(),
               s.resolution(),
               s.powerUsage());
    }
}

bool printSensors(SfwSensorManager &mgr)
{
    // Use SFW API: list sensors
    Vector<SfwSensor> sensors;
    ssize_t count = mgr.getSensorList(sensors);
    printf("%d sensors found.\n", int(count));

    if (count != (int)sensors.size()) {
        printf("BUG!\n");
        return false;
    }

    for (int i = 0, n = sensors.size(); i < n; ++i) {
        const SfwSensor &s = sensors[i];

        printf("Sensor %d:\n", i);
        printSensorInfo(s, false);
    }
    return true;
}


int KlaatuSensor::sensorEventThread()
{

    printf("ENTERING polling loop...\n\n");
    #if 0
    printf("%sEVNT#  EVENT_TS us/+DT ms (dtyp NN)  EVENT_DATA\n-----------"
           "----------------------------------------------------------------------------------\n",
           showTimes ? "[ SYSTS usec ] + DELTA us| " : "");
    #endif

    for(;;) {
        //printf("about to poll...\n");
        int32_t ret;
        // alternativelly:
        ret = mloop->pollOnce(-1);
        switch (ret) {
            case ALOOPER_POLL_WAKE:
                printf("ALOOPER_POLL_WAKE\n");
                break;
            case ALOOPER_POLL_CALLBACK:
                //printf("ALOOPER_POLL_CALLBACK\n");
                break;
            case ALOOPER_POLL_TIMEOUT:
                printf(".\n");
                break;
            case ALOOPER_POLL_ERROR:
                printf("ALOOPER_POLL_TIMEOUT\n");
                break;
            default:
                printf("ugh? poll returned %d\n", ret);
                break;
        }
        if(!keepRunning){
                printf("TEST deactivation of sensor %s: ", sensor->name().string());
                status_t r = mgr.disableSensor(sensor->handle());
                printf("result: %d\n", r);
		//break;
        }
    }

    printf("%s: Exit \n", __func__);
	
    return 0;
}

static int beginSensorThread(void *cookie)
{
    return static_cast<KlaatuSensor *>(cookie)->sensorEventThread();
}

EvHandler receiver;
int KlaatuSensor::initSensor(int32_t sensor_type)
{
    bool observer = false;
    int eventRate = 100;  // Hz
    // Configure & start sensors

    if (!mgr.initialized()) {
        fprintf(stderr, "Sensor manager not initialized. Service not running?\n");
        return EXIT_FAILURE;
    }

    if (!sensor) {
        fprintf(stderr, "Error: sensor matching \"%s\" not found.\n", "acc");
        return EXIT_FAILURE;
    }


    keepRunning = true;

    int mode = (observer ? sfw::SensorOptObserver : sfw::SensorOptNone);
    int32_t sHandle = sensor->handle();

    printf("Enabling sensor (%d):\n", sHandle);
    printSensorInfo(*sensor);

    nsecs_t ts = tsCheck("\n");
    
    startedAt = ts;

    status_t err = mgr.enableSensor(sHandle, 0, mode);
    tsCheck("");
    printf("Sensor \"%s\" enabled, err = %d\n", sensor->name().string(), int(err));
    XASSERT(err == 0);

    if (err == 0 && eventRate) {
        err = mgr.setEventRate(sensor, 1000000 / eventRate);
        if (err) {
            printf("setEventRate failed. Status %d\n", err);
        }
    }


    printf("%d THE END\n", __LINE__);
    return EXIT_SUCCESS;
}
void KlaatuSensor::exitSensor(int32_t sensor_type)
{
	if (sensor_type == Sensor::TYPE_ACCELEROMETER ){
		keepRunning = false;
	}
    if (csvFile) {
        fclose(csvFile);
        csvFile = 0;
    }
}
KlaatuSensor::KlaatuSensor(/*ScreenControl *s*/)
{
    int c;
    //bool keepRunning = true,
    bool sensorListingRequested = false;
    unsigned int maxEvCount = 10;
    bool autoExit = true;

    tsCheck("START\n");

    mCookie = this;
    // If no snesor selected, set the default
    if (!sensor_type)
        sensor_type = Sensor::TYPE_ACCELEROMETER;

    mloop= new Looper(false);
	

    // Sensor manager integrates with provided Looper; event I/O is done via polling
    mgr.initialize(mloop);
    if (!mgr.initialized()) {
        fprintf(stderr, "Sensor manager not initialized. Service not running?\n");
        return ;
    }

    tsCheck("SfwSensorManager created\n");

    nsensors = 1;

    if (const sensor_desc *desc = selectSensorByName("acc")) {
        // multiple sensors are of same type will be loaded by their names
        sensor = desc->getByName ? mgr.getSensor(desc->name)
                                : mgr.getDefaultSensor(desc->type);
        if (!sensor) {
            fprintf(stderr, "Error: sensor type %d not available\n", desc->type);
        }
    }

    if (!sensor) {
        fprintf(stderr, "Error: sensor matching \"%s\" not found.\n", "acc");
        return ;
    }

    printf("USING sensor (%s), type %d\n",
           sensor->name().string(),
           sensor->sensorType());


    // All sensor events will be dispatched to this handler
    mgr.setEventHandler(&receiver);


    if (createThread(beginSensorThread, this) == false) 
    {
        //LOG_ALWAYS_FATAL("ERROR!  Unable to create Sensor thread \n");
        printf("ERROR!  Unable to create Sensor thread \n");
    }

}

KlaatuSensor::~KlaatuSensor(/*ScreenControl *s*/)
{
	printf("%s: setting keepRunning to FALSE \n", __func__);
	keepRunning = false;
}
