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

//#define SENSOR_EVENT_DEBUG

#include <sensors/sensor.h>

static nsecs_t sStartTime = 0;


int onSensorChanged(ASensorEvent buffer);


namespace android {


/*
 * This is a Hack or may be not ????
 * 
 * receiver() is not in KlaatuSensor class, so the compiler does not allow it to use a
 * callback function defined in the KlaatuSensor Class if it's not static
 * if the callback is declared static, that makes compiler happy BUT the linker complains
 * about it as undefined
 *
 * to get around this problem we define a void *
 * initialise it with "this" in KlaatuSensor::initSensor()
 * and call it as 
 *   static_cast<KlaatuSensor *>(mCookie)->sensor_event_handler(Sensor::TYPE_ACCELEROMETER, rotation);
 * in receiver() .
 */
void *mCookie=NULL;

int receiver(int fd, int events, void* data)
{
    sp<SensorEventQueue> q((SensorEventQueue*)data);
    ssize_t n;
    ASensorEvent buffer[8];

    static nsecs_t oldTimeStamp = 0;

    //printf("%s In\n", __FUNCTION__);

    while ((n = q->read(buffer, 8)) > 0) {
        for (int i=0 ; i<n ; i++) {
            float t;
            if (oldTimeStamp) {
                t = float(buffer[i].timestamp - oldTimeStamp) / s2ns(1);
            } else {
                t = float(buffer[i].timestamp - sStartTime) / s2ns(1);
            }
            oldTimeStamp = buffer[i].timestamp;

            if (buffer[i].type == Sensor::TYPE_ACCELEROMETER) {
                #ifdef SENSOR_EVENT_DEBUG
                printf("%lld\t%8f\t%8f\t%8f\t%f\n",
                        buffer[i].timestamp,
                        buffer[i].data[0], buffer[i].data[1], buffer[i].data[2],
                        1.0/t);
                #endif
                int rotation = onSensorChanged(buffer[i]);
                #if 1
                if (rotation >= 0) {
                    if (static_cast<KlaatuSensor *>(mCookie)->sensor_event_handler)
                        static_cast<KlaatuSensor *>(mCookie)->sensor_event_handler(Sensor::TYPE_ACCELEROMETER, rotation);
                }
                #endif
            }
			
            /*
             * Add your sensor processing here
             * Check sensor
             * and function to process events
             */
            else if (buffer[i].type == Sensor::TYPE_GYROSCOPE) {
                //#ifdef SENSOR_EVENT_DEBUG
                printf("%lld\t%8f\t%8f\t%8f\t%f\n",
                    buffer[i].timestamp,
                    buffer[i].data[0], buffer[i].data[1], buffer[i].data[2],
                    1.0/t);
                //#endif
                //onSensorChanged(buffer[i]);
            }

        }
    }
    if (n<0 && n != -EAGAIN) {
        printf("error reading events (%s)\n", strerror(-n));
    }
    return 1;
}

int KlaatuSensor::sensorEventThread()
{
    static int oldRotation=0;
    do {
        //printf("about to poll... \n");
        int32_t ret = mloop->pollOnce(-1);
        switch (ret) {
            case ALOOPER_POLL_WAKE:
                //printf("ALOOPER_POLL_WAKE\n");
                break;
            case ALOOPER_POLL_CALLBACK:
                //printf("ALOOPER_POLL_CALLBACK\n");
                break;
            case ALOOPER_POLL_TIMEOUT:
                printf("ALOOPER_POLL_TIMEOUT\n");
                break;
            case ALOOPER_POLL_ERROR:
                printf("ALOOPER_POLL_TIMEOUT\n");
                break;
            default:
                printf("ugh? poll returned %d\n", ret);
                break;
        }
    } while (1);

    //printf("%s Exiting...\n", __FUNCTION__);
	
    return 0;
}

static int beginSensorThread(void *cookie)
{
    //printf("%s\n", __FUNCTION__);
    return static_cast<KlaatuSensor *>(cookie)->sensorEventThread();
}

void KlaatuSensor::initSensor(int32_t sensor_type)
{
    mCookie = this;
    // If no snesor selected, set the default
    if (!sensor_type)
        sensor_type = Sensor::TYPE_ACCELEROMETER;

    // Check if sensor is ssupported yet..

    SensorManager& mgr(SensorManager::getInstance());

    Sensor const* const* list;
    ssize_t count = mgr.getSensorList(&list);
    printf("numSensors=%d\n", int(count));
    for (size_t i=0 ; i<size_t(count) ; i++) {
        printf("Name: %s, Type=%d, Vendor: %s\n", list[i]->getName().string(), list[i]->getType(), list[i]->getVendor().string());
    }

    mq = mgr.createEventQueue();
    printf("queue=%p\n", mq.get());

    if (sensor_type & Sensor::TYPE_ACCELEROMETER)
    {
        Sensor const* accelerometer = mgr.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
        printf("accelerometer=%p (%s)\n",
                accelerometer, accelerometer->getName().string());
        
        sStartTime = systemTime();
        
        mq->enableSensor(accelerometer);
        
        //mq->setEventRate(accelerometer, ms2ns(2));
        mq->setEventRate(accelerometer, ms2ns(10));
    }

    /*
     *  ADD Your Sensor Here
     */
    if (sensor_type & Sensor::TYPE_GYROSCOPE)
    {
        Sensor const* gyroscope = mgr.getDefaultSensor(Sensor::TYPE_GYROSCOPE);
        printf("gyroscope=%p (%s)\n",
                gyroscope, gyroscope->getName().string());
        
        sStartTime = systemTime();
        
        mq->enableSensor(gyroscope);
        
        mq->setEventRate(gyroscope, ms2ns(10));
    }

    mloop= new Looper(false);
	
    mloop->addFd(mq->getFd(), 0, ALOOPER_EVENT_INPUT, receiver, mq.get());

    if (createThread(beginSensorThread, this) == false) 
	{
        //LOG_ALWAYS_FATAL("ERROR!  Unable to create Sensor thread \n");
        printf("ERROR!  Unable to create Sensor thread \n");
	}

}
void KlaatuSensor::exitSensor(int32_t sensor_type)
{
}
KlaatuSensor::KlaatuSensor(/*ScreenControl *s*/)
{
    //sensor_event_handler = NULL;
    //int32_t sensor_type = Sensor::TYPE_ACCELEROMETER;
    //initSensor(sensor_type);
}
KlaatuSensor::~KlaatuSensor(/*ScreenControl *s*/)
{
}


};
