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

#include <math.h>

#include <sensors/sensor_includes.h>
#include <utils/KeyedVector.h>


#include <hardware/sensors.h>

#if 0
#include <QDebug>
#endif
#include <sensors/sensor_includes.h>

using namespace android;

#define LOG 0
#define LOG1 0

#define MIN_VALUE -2147483647
#if 0
typedef struct SE {
	float values[3];
	int64_t timestamp;
} SensorEvent;
#endif
int onSensorChanged(ASensorEvent buffer);


// The gap angle in degrees between adjacent orientation angles for hysteresis.
// This creates a "dead zone" between the current orientation and a proposed
// adjacent orientation.  No orientation proposal is made when the orientation
// angle is within the gap between the current orientation and the adjacent
// orientation.
#define ADJACENT_ORIENTATION_ANGLE_GAP  45

int mCurrentRotation=0;
// Timestamp and value of the last accelerometer sample.
long long mLastFilteredTimestampNanos;
float mLastFilteredX, mLastFilteredY, mLastFilteredZ;

// The last proposed rotation, -1 if unknown.
int mProposedRotation;
// Timestamp when the device last appeared to be flat for sure (the flat delay elapsed).
long long mFlatTimestampNanos;
// Timestamp when the device last appeared to be swinging.
long long mSwingTimestampNanos;
// Timestamp when the device last appeared to be undergoing external acceleration.
long long mAccelerationTimestampNanos;
// Value of the current predicted rotation, -1 if unknown.
int mPredictedRotation;
// Timestamp of when the predicted rotation most recently changed.
long long mPredictedRotationTimestampNanos;

// We work with all angles in degrees in this class.
#define RADIANS_TO_DEGREES  ((float) (180 / (22/7)))

// Number of nanoseconds per millisecond.
#define NANOS_PER_MS  1000000

// Indices into SensorEvent.values for the accelerometer sensor.
#define ACCELEROMETER_DATA_X  0
#define ACCELEROMETER_DATA_Y  1
#define ACCELEROMETER_DATA_Z  2
// The minimum amount of time that a predicted rotation must be stable before it
// is accepted as a valid rotation proposal.  This value can be quite small because
// the low-pass filter already suppresses most of the noise so we're really just
// looking for quick confirmation that the last few samples are in agreement as to
// the desired orientation.
#define PROPOSAL_SETTLE_TIME_NANOS  40 * NANOS_PER_MS

// The minimum amount of time that must have elapsed since the device last exited
// the flat state (time since it was picked up) before the proposed rotation
// can change.
#define PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS  500 * NANOS_PER_MS

// The minimum amount of time that must have elapsed since the device stopped
// swinging (time since device appeared to be in the process of being put down
// or put away into a pocket) before the proposed rotation can change.
#define PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS  300 * NANOS_PER_MS

// The minimum amount of time that must have elapsed since the device stopped
// undergoing external acceleration before the proposed rotation can change.
#define PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS  500 * NANOS_PER_MS

// If the tilt angle remains greater than the specified angle for a minimum of
// the specified time, then the device is deemed to be lying flat
// (just chillin' on a table).
#define FLAT_ANGLE  75
#define FLAT_TIME_NANOS  1000 * NANOS_PER_MS

// If the tilt angle has increased by at least delta degrees within the specified amount
// of time, then the device is deemed to be swinging away from the user
// down towards flat (tilt = 90).
#define SWING_AWAY_ANGLE_DELTA  20
#define SWING_TIME_NANOS  (300 * NANOS_PER_MS)


// The maximum sample inter-arrival time in milliseconds.
// If the acceleration samples are further apart than this amount in time, we reset the
// state of the low-pass filter and orientation properties.  This helps to handle
// boundary conditions when the device is turned on, wakes from suspend or there is
// a significant gap in samples.
#define MAX_FILTER_DELTA_TIME_NANOS  (1000 * NANOS_PER_MS)

// The acceleration filter time constant.
//
// This time constant is used to tune the acceleration filter such that
// impulses and vibrational noise (think car dock) is suppressed before we
// try to calculate the tilt and orientation angles.
//
// The filter time constant is related to the filter cutoff frequency, which is the
// frequency at which signals are attenuated by 3dB (half the passband power).
// Each successive octave beyond this frequency is attenuated by an additional 6dB.
//
// Given a time constant t in seconds, the filter cutoff frequency Fc in Hertz
// is given by Fc = 1 / (2pi * t).
//
// The higher the time constant, the lower the cutoff frequency, so more noise
// will be suppressed.
//
// Filtering adds latency proportional the time constant (inversely proportional
// to the cutoff frequency) so we don't want to make the time constant too
// large or we can lose responsiveness.  Likewise we don't want to make it too
// small or we do a poor job suppressing acceleration spikes.
// Empirically, 100ms seems to be too small and 500ms is too large.
#define FILTER_TIME_CONSTANT_MS  200.0

/* State for orientation detection. */

// Thresholds for minimum and maximum allowable deviation from gravity.
//
// If the device is undergoing external acceleration (being bumped, in a car
// that is turning around a corner or a plane taking off) then the magnitude
// may be substantially more or less than gravity.  This can skew our orientation
// detection by making us think that up is pointed in a different direction.
//
// Conversely, if the device is in freefall, then there will be no gravity to
// measure at all.  This is problematic because we cannot detect the orientation
// without gravity to tell us which way is up.  A magnitude near 0 produces
// singularities in the tilt and orientation calculations.
//
// In both cases, we postpone choosing an orientation.
//
// However, we need to tolerate some acceleration because the angular momentum
// of turning the device can skew the observed acceleration for a short period of time.

/** Standard gravity (g) on Earth. This value is equivalent to 1G */
#define STANDARD_GRAVITY  9.80665
#define NEAR_ZERO_MAGNITUDE  0.9 // m/s^2
#define ACCELERATION_TOLERANCE  4 // m/s^2
#define MIN_ACCELERATION_MAGNITUDE  (STANDARD_GRAVITY - ACCELERATION_TOLERANCE)
#define MAX_ACCELERATION_MAGNITUDE (STANDARD_GRAVITY + ACCELERATION_TOLERANCE)

// Maximum absolute tilt angle at which to consider orientation data.  Beyond this (i.e.
// when screen is facing the sky or ground), we completely ignore orientation data.
#define MAX_TILT  75

// The tilt angle range in degrees for each orientation.
// Beyond these tilt angles, we don't even consider transitioning into the
// specified orientation.  We place more stringent requirements on unnatural
// orientations than natural ones to make it less likely to accidentally transition
// into those states.
// The first value of each pair is negative so it applies a limit when the device is
// facing down (overhead reading in bed).
// The second value of each pair is positive so it applies a limit when the device is
// facing up (resting on a table).
// The ideal tilt angle is 0 (when the device is vertical) so the limits establish
// how close to vertical the device must be in order to change orientation.
int TILT_TOLERANCE[4][2] = {
    /* ROTATION_0   */ { -25, 70 },
    /* ROTATION_90  */ { -25, 65 },
    /* ROTATION_180 */ { -25, 60 },
    /* ROTATION_270 */ { -25, 65 }
};

// History of observed tilt angles.
#define TILT_HISTORY_SIZE  40
float mTiltHistory[TILT_HISTORY_SIZE];
long long mTiltHistoryTimestampNanos[TILT_HISTORY_SIZE];
int mTiltHistoryIndex;

int nextTiltHistoryIndex(int index) {
    index = (index == 0 ? TILT_HISTORY_SIZE : index) - 1;
    return mTiltHistoryTimestampNanos[index] != (long long) MIN_VALUE ? index : -1;
}

static float remainingMS(long long now, long long until) {
    return now >= until ? 0 : (until - now) * 0.000001f;
}

void updatePredictedRotation(long long now, int rotation) {
    if (mPredictedRotation != rotation) {
        mPredictedRotation = rotation;
        mPredictedRotationTimestampNanos = now;
    }
}

bool isAcceleratingF(float magnitude) {
    return magnitude < MIN_ACCELERATION_MAGNITUDE
            || magnitude > MAX_ACCELERATION_MAGNITUDE;
}
bool isFlatF(long long now) {
    for (int i = mTiltHistoryIndex; (i = nextTiltHistoryIndex(i)) >= 0; ) {
        if (mTiltHistory[i] < FLAT_ANGLE) {
            break;
        }
        if (mTiltHistoryTimestampNanos[i] + FLAT_TIME_NANOS <= now) {
            // Tilt has remained greater than FLAT_TILT_ANGLE for FLAT_TIME_NANOS.
            return true;
        }
    }
    return false;
}

bool isSwingingF(long long now, float tilt) {
    for (int i = mTiltHistoryIndex; (i = nextTiltHistoryIndex(i)) >= 0; ) {
        if (mTiltHistoryTimestampNanos[i] + SWING_TIME_NANOS < now) {
            break;
        }
        if (mTiltHistory[i] + SWING_AWAY_ANGLE_DELTA <= tilt) {
            // Tilted away by SWING_AWAY_ANGLE_DELTA within SWING_TIME_NANOS.
            return true;
        }
    }
    return false;
}
void addTiltHistoryEntry(long long now, float tilt) {
    mTiltHistory[mTiltHistoryIndex] = tilt;
    mTiltHistoryTimestampNanos[mTiltHistoryIndex] = now;
    mTiltHistoryIndex = (mTiltHistoryIndex + 1) % TILT_HISTORY_SIZE;
    mTiltHistoryTimestampNanos[mTiltHistoryIndex] = MIN_VALUE;
}

void clearTiltHistory() {
	mTiltHistoryTimestampNanos[0] = MIN_VALUE;
	mTiltHistoryIndex = 1;
}
void clearPredictedRotation() {
	mPredictedRotation = -1;
	mPredictedRotationTimestampNanos = MIN_VALUE;
}

/**
 * Returns true if the tilt angle is acceptable for a given predicted rotation.
 */
bool isTiltAngleAcceptable(int rotation, int tiltAngle) {
    return tiltAngle >= TILT_TOLERANCE[rotation][0]
            && tiltAngle <= TILT_TOLERANCE[rotation][1];
}

/**
 * Returns true if the orientation angle is acceptable for a given predicted rotation.
 *
 * This function takes into account the gap between adjacent orientations
 * for hysteresis.
 */
bool isOrientationAngleAcceptable(int rotation, int orientationAngle) {
    // If there is no current rotation, then there is no gap.
    // The gap is used only to introduce hysteresis among advertised orientation
    // changes to avoid flapping.
    int currentRotation = mCurrentRotation;
    if (currentRotation >= 0) {
        // If the specified rotation is the same or is counter-clockwise adjacent
        // to the current rotation, then we set a lower bound on the orientation angle.
        // For example, if currentRotation is ROTATION_0 and proposed is ROTATION_90,
        // then we want to check orientationAngle > 45 + GAP / 2.
        if (rotation == currentRotation
                || rotation == (currentRotation + 1) % 4) {
            int lowerBound = rotation * 90 - 45
                    + ADJACENT_ORIENTATION_ANGLE_GAP / 2;
            if (rotation == 0) {
                if (orientationAngle >= 315 && orientationAngle < lowerBound + 360) {
                    return false;
                }
            } else {
                if (orientationAngle < lowerBound) {
                    return false;
                }
            }
        }

        // If the specified rotation is the same or is clockwise adjacent,
        // then we set an upper bound on the orientation angle.
        // For example, if currentRotation is ROTATION_0 and rotation is ROTATION_270,
        // then we want to check orientationAngle < 315 - GAP / 2.
        if (rotation == currentRotation
                || rotation == (currentRotation + 3) % 4) {
            int upperBound = rotation * 90 + 45
                    - ADJACENT_ORIENTATION_ANGLE_GAP / 2;
            if (rotation == 0) {
                if (orientationAngle <= 45 && orientationAngle > upperBound) {
                    return false;
                }
            } else {
                if (orientationAngle > upperBound) {
                    return false;
                }
            }
        }
    }
    return true;
}
/**
 * Returns true if the predicted rotation is ready to be advertised as a
 * proposed rotation.
 */
bool isPredictedRotationAcceptable(long long now) {
    // The predicted rotation must have settled long enough.
    if (now < mPredictedRotationTimestampNanos + PROPOSAL_SETTLE_TIME_NANOS) {
		printf("%d: %lld < %lld + %d\n", __LINE__, now, mPredictedRotationTimestampNanos, PROPOSAL_SETTLE_TIME_NANOS);
        return false;
    }

    // The last flat state (time since picked up) must have been sufficiently long ago.
    if (now < mFlatTimestampNanos + PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS) {
		printf("%d: %lld < %lld + %d\n", __LINE__, now, mFlatTimestampNanos, PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS);
        return false;
    }

    // The last swing state (time since last movement to put down) must have been
    // sufficiently long ago.
    if (now < mSwingTimestampNanos + PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS) {
		printf("%d: %lld < %lld + %d\n", __LINE__, now, mSwingTimestampNanos, PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS);
        return false;
    }

#if 0
    // The last acceleration state must have been sufficiently long ago.
    if (now < mAccelerationTimestampNanos
            + PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS) {
		printf("%d: %lld < %lld + %d\n", __LINE__, now, mAccelerationTimestampNanos, PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS);
        return false;
    }
#endif

    // Looks good!
    return true;
}

void reset() {
	mLastFilteredTimestampNanos = MIN_VALUE;
	mProposedRotation = -1;
	mFlatTimestampNanos = MIN_VALUE;
	mSwingTimestampNanos = MIN_VALUE;
	mAccelerationTimestampNanos = MIN_VALUE;
	clearPredictedRotation();
	clearTiltHistory();
}

int onSensorChanged(ASensorEvent buffer)
{
    // The vector given in the SensorEvent points straight up (towards the sky) under ideal
    // conditions (the phone is not accelerating).  I'll call this up vector elsewhere.
    float x = buffer.acceleration.x;
    float y = buffer.acceleration.y;
    float z = buffer.acceleration.z;

    if (LOG) {
		printf("Raw acceleration vector: x= %f, y= %f, z= %f,  magnitude= %f\n", x,y,z, sqrt(x * x + y * y + z * z));
    }

    // Apply a low-pass filter to the acceleration up vector in cartesian space.
    // Reset the orientation listener state if the samples are too far apart in time
    // or when we see values of (0, 0, 0) which indicates that we polled the
    // accelerometer too soon after turning it on and we don't have any data yet.
    long long now = buffer.timestamp;
    long long then = mLastFilteredTimestampNanos;
    float timeDeltaMS = (now - then) * 0.000001f;
    bool skipSample;
    if (now < then
            || now > then + MAX_FILTER_DELTA_TIME_NANOS
            || (x == 0 && y == 0 && z == 0)) {
        if (LOG) {
            printf("Resetting orientation listener.\n");
        }
        reset();
        skipSample = true;
    } else {
        float alpha = timeDeltaMS / (FILTER_TIME_CONSTANT_MS + timeDeltaMS);
        x = alpha * (x - mLastFilteredX) + mLastFilteredX;
        y = alpha * (y - mLastFilteredY) + mLastFilteredY;
        z = alpha * (z - mLastFilteredZ) + mLastFilteredZ;
        if (LOG) {
			printf("Filtered acceleration vector: x= %f, y= %f, z= %f,  magnitude= %f\n", x,y,z, sqrt(x * x + y * y + z * z));
        }
        skipSample = false;
    }
    mLastFilteredTimestampNanos = now;
    mLastFilteredX = x;
    mLastFilteredY = y;
    mLastFilteredZ = z;

    bool isAccelerating = false;
    bool isFlat = false;
    bool isSwinging = false;
    if (!skipSample) {
        // Calculate the magnitude of the acceleration vector.
        float magnitude = sqrt(x * x + y * y + z * z);
        if (magnitude < NEAR_ZERO_MAGNITUDE) {
            if (LOG) {
                //printf("Ignoring sensor data, magnitude %f too close to zero.\n", magnitude);
            }
            clearPredictedRotation();
        } else {
            // Determine whether the device appears to be undergoing external acceleration.
            if (isAcceleratingF(magnitude)) {
                isAccelerating = true;
                mAccelerationTimestampNanos = now;
            }

            // Calculate the tilt angle.
            // This is the angle between the up vector and the x-y plane (the plane of
            // the screen) in a range of [-90, 90] degrees.
            //   -90 degrees: screen horizontal and facing the ground (overhead)
            //     0 degrees: screen vertical
            //    90 degrees: screen horizontal and facing the sky (on table)
            int tiltAngle = (int) round(
                    asin(z / magnitude) * RADIANS_TO_DEGREES);
            addTiltHistoryEntry(now, tiltAngle);

            // Determine whether the device appears to be flat or swinging.
            if (isFlatF(now)) {
                isFlat = true;
                mFlatTimestampNanos = now;
            }
            if (isSwingingF(now, tiltAngle)) {
                isSwinging = true;
                mSwingTimestampNanos = now;
            }

            // If the tilt angle is too close to horizontal then we cannot determine
            // the orientation angle of the screen.
            if (abs(tiltAngle) > MAX_TILT) {
                if (LOG) {
                    printf("Ignoring sensor data, tilt angle too high: tiltAngle= %d\n", tiltAngle);
                }
                clearPredictedRotation();
            } else {
                // Calculate the orientation angle.
                // This is the angle between the x-y projection of the up vector onto
                // the +y-axis, increasing clockwise in a range of [0, 360] degrees.
                int orientationAngle = (int) round(
                        -atan2(-x, y) * RADIANS_TO_DEGREES);
                if (orientationAngle < 0) {
                    // atan2 returns [-180, 180]; normalize to [0, 360]
                    orientationAngle += 360;
                }

                // Find the nearest rotation.
                int nearestRotation = (orientationAngle + 45) / 90;
                if (nearestRotation == 4) {
                    nearestRotation = 0;
                }

                // Determine the predicted orientation.
                if (isTiltAngleAcceptable(nearestRotation, tiltAngle)
                        && isOrientationAngleAcceptable(nearestRotation,
                                orientationAngle))
				{
                    updatePredictedRotation(now, nearestRotation);
                    if (LOG) {
                        #if 0
                        Slog.v(TAG, "Predicted: "
                                + "tiltAngle=" + tiltAngle
                                + ", orientationAngle=" + orientationAngle
                                + ", predictedRotation=" + mPredictedRotation
                                + ", predictedRotationAgeMS="
                                        + ((now - mPredictedRotationTimestampNanos)
                                                * 0.000001f));
                        #endif
                        #if 1
                        printf("Predicted: tiltAngle= %d, orientationAngle= %d, predictedRotation= %d,  predictedRotationAgeMS=%lf\n",
                            tiltAngle, orientationAngle, mPredictedRotation, + ((now - mPredictedRotationTimestampNanos) * 0.000001f));
						#endif
                    }
                } else {
                    if (LOG) {
                        #if 0
                        Slog.v(TAG, "Ignoring sensor data, no predicted rotation: "
                                + "tiltAngle=" + tiltAngle
                                + ", orientationAngle=" + orientationAngle);
                        #endif
                        #if 1
                        printf("Ignoring sensor data, no predicted rotation: tiltAngle= %d, orientationAngle= %d\n", tiltAngle, orientationAngle);
                        #endif
                    }
                    clearPredictedRotation();
                }
            }
        }
    }

    // Determine new proposed rotation.
    int oldProposedRotation = mProposedRotation;
    if (mPredictedRotation < 0 || isPredictedRotationAcceptable(now)) {
        mProposedRotation = mPredictedRotation;
    }

    // Write final statistics about where we are in the orientation detection process.
    if (LOG) {
        #if 0
        Slog.v(TAG, "Result: currentRotation=" + mOrientationListener.mCurrentRotation
                + ", proposedRotation=" + mProposedRotation
                + ", predictedRotation=" + mPredictedRotation
                + ", timeDeltaMS=" + timeDeltaMS
                + ", isAccelerating=" + isAccelerating
                + ", isFlat=" + isFlat
                + ", isSwinging=" + isSwinging
                + ", timeUntilSettledMS=" + remainingMS(now,
                        mPredictedRotationTimestampNanos + PROPOSAL_SETTLE_TIME_NANOS)
                + ", timeUntilAccelerationDelayExpiredMS=" + remainingMS(now,
                        mAccelerationTimestampNanos + PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS)
                + ", timeUntilFlatDelayExpiredMS=" + remainingMS(now,
                        mFlatTimestampNanos + PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS)
                + ", timeUntilSwingDelayExpiredMS=" + remainingMS(now,
                        mSwingTimestampNanos + PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS));
        #else
        #if 1
        printf("Result: currentRotation= %d",  mCurrentRotation);
        printf(", proposedRotation= %d", mProposedRotation);
        printf(", predictedRotation= %d", mPredictedRotation);
        printf(", timeDeltaMS= %lf", timeDeltaMS);
        printf(", isAccelerating= %d", isAccelerating);
        printf(", isFlat= %d", isFlat);
        printf(", isSwinging= %d", isSwinging);
		printf("\n");
        printf(", timeUntilSettledMS= %lf", remainingMS(now, mPredictedRotationTimestampNanos + PROPOSAL_SETTLE_TIME_NANOS));
        printf(", timeUntilAccelerationDelayExpiredMS= %lf", remainingMS(now, mAccelerationTimestampNanos + PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS));
        printf(", timeUntilFlatDelayExpiredMS= %lf", remainingMS(now, mFlatTimestampNanos + PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS));
        printf(", timeUntilSwingDelayExpiredMS= %lf", remainingMS(now, mSwingTimestampNanos + PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS));
		printf("\n");
        #endif
        #endif
    }

    // Tell the listener.
    if (mProposedRotation != oldProposedRotation && mProposedRotation >= 0) {
        if (LOG1) {
            #if 0
            Slog.v(TAG, "Proposed rotation changed!  proposedRotation=" + mProposedRotation
                    + ", oldProposedRotation=" + oldProposedRotation);
            #else
            printf("Proposed rotation changed!  proposedRotation= %d",  mProposedRotation);
            printf(", oldProposedRotation= %d\n", oldProposedRotation);
            #endif
			}

        return mProposedRotation;
    }

    return -1;
}

