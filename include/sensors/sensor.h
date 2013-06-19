
#ifndef _KLAATU_SENSOR_H
#define _KLAATU_SENSOR_H

#include <sensors/sensor_includes.h>

#include <utils/List.h>

namespace android {

typedef struct KS {
	const char *name;
	int32_t    type;
} klaatu_sensor_list_t;

/*
 * Uncomment your sensor
 */

klaatu_sensor_list_t klaatuSensors[] = {
	{"accel", Sensor::TYPE_ACCELEROMETER},
	//{"mag", Sensor::TYPE_MAGNETIC_FIELD},
	//{"gyro", Sensor::TYPE_GYROSCOPE},
	//{"light", Sensor::TYPE_LIGHT},
	//{"proximity", Sensor::TYPE_PROXIMITY},
	{NULL, 0}
};
class KlaatuSensor
{

public:
	KlaatuSensor();
	~KlaatuSensor() {};

	int sensor_type;
	// TODO: Make it more generic for other sensors

	void (*sensor_event_handler)(int sensor_type, int value);

	int         sensorEventThread();
	void        initSensor(int32_t sensor_type);


private:
	sp<SensorEventQueue> mq;
	sp<Looper> mloop;
 };

}; // namespace android

#endif // _KLAATU_SENSOR_H
