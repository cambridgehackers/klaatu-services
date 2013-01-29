#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <binder/ProcessState.h>
#include <binder/BinderService.h>
#include <media/IMediaPlayerClient.h>
#include <media/IMediaPlayerService.h>
#include <media/AudioSystem.h>
#include <media/ToneGenerator.h>
#include <media/mediaplayer.h>

using namespace android;

// -------------------------------------------------------------------

/* Max volume index values for audio streams.
   Copied from AudioService.java
*/

const int sMaxStreamVolume[] = {
     5,  // AUDIO_STREAM_VOICE_CALL
     7,  // AUDIO_STREAM_SYSTEM
     7,  // AUDIO_STREAM_RING
     15, // AUDIO_STREAM_MUSIC
     7,  // AUDIO_STREAM_ALARM
     7,  // AUDIO_STREAM_NOTIFICATION
     15, // AUDIO_STREAM_BLUETOOTH_SCO
     7,  // AUDIO_STREAM_SYSTEM_ENFORCED
     15, // AUDIO_STREAM_DTMF
     15  // AUDIO_STREAM_TTS
};

/* Default volume index values for audio streams */

const int sDefaultStreamVolume[] = {
    4,  // AUDIO_STREAM_VOICE_CALL
    7,  // AUDIO_STREAM_SYSTEM
    5,  // AUDIO_STREAM_RING
    11, // AUDIO_STREAM_MUSIC
    6,  // AUDIO_STREAM_ALARM
    5,  // AUDIO_STREAM_NOTIFICATION
    7,  // AUDIO_STREAM_BLUETOOTH_SCO
    7,  // AUDIO_STREAM_SYSTEM_ENFORCED
    11, // AUDIO_STREAM_DTMF
    11  // AUDIO_STREAM_TTS
};

/* 
   Indicates that a stream uses the volume settings of another stream

   STREAM_VOLUME_ALIAS[] indicates for each stream if it uses the volume settings
   of another stream: This avoids multiplying the volume settings for hidden
   stream types that follow other stream behavior for volume settings
   NOTE: do not create loops in aliases! 
*/

const audio_stream_type_t sStreamVolumeAlias[] = {
    AUDIO_STREAM_VOICE_CALL,    // AUDIO_STREAM_VOICE_CALL
    AUDIO_STREAM_SYSTEM,        // AUDIO_STREAM_SYSTEM
    AUDIO_STREAM_RING,          // AUDIO_STREAM_RING
    AUDIO_STREAM_MUSIC,         // AUDIO_STREAM_MUSIC
    AUDIO_STREAM_ALARM,         // AUDIO_STREAM_ALARM
    AUDIO_STREAM_RING,          // AUDIO_STREAM_NOTIFICATION
    AUDIO_STREAM_BLUETOOTH_SCO, // AUDIO_STREAM_BLUETOOTH_SCO
    AUDIO_STREAM_SYSTEM,        // AUDIO_STREAM_SYSTEM_ENFORCED
    AUDIO_STREAM_VOICE_CALL,    // AUDIO_STREAM_DTMF
    AUDIO_STREAM_MUSIC          // AUDIO_STREAM_TTS
};

const char *DEVICE_OUT_EARPIECE_NAME = "earpiece";
const char *DEVICE_OUT_SPEAKER_NAME = "speaker";
const char *DEVICE_OUT_WIRED_HEADSET_NAME = "headset";
const char *DEVICE_OUT_WIRED_HEADPHONE_NAME = "headphone";
const char *DEVICE_OUT_BLUETOOTH_SCO_NAME = "bt_sco";
const char *DEVICE_OUT_BLUETOOTH_SCO_HEADSET_NAME = "bt_sco_hs";
const char *DEVICE_OUT_BLUETOOTH_SCO_CARKIT_NAME = "bt_sco_carkit";
const char *DEVICE_OUT_BLUETOOTH_A2DP_NAME = "bt_a2dp";
const char *DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES_NAME = "bt_a2dp_hp";
const char *DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER_NAME = "bt_a2dp_spk";
const char *DEVICE_OUT_AUX_DIGITAL_NAME = "aux_digital";
const char *DEVICE_OUT_ANLG_DOCK_HEADSET_NAME = "analog_dock";
const char *DEVICE_OUT_DGTL_DOCK_HEADSET_NAME = "digital_dock";
const char *DEVICE_OUT_USB_ACCESSORY_NAME = "usb_accessory";
const char *DEVICE_OUT_USB_DEVICE_NAME = "usb_device";

const char *getDeviceName(audio_devices_t device)
{
    switch(device) {
    case AUDIO_DEVICE_OUT_EARPIECE:
	return DEVICE_OUT_EARPIECE_NAME;
    case AUDIO_DEVICE_OUT_SPEAKER:
	return DEVICE_OUT_SPEAKER_NAME;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
	return DEVICE_OUT_WIRED_HEADSET_NAME;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
	return DEVICE_OUT_WIRED_HEADPHONE_NAME;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
	return DEVICE_OUT_BLUETOOTH_SCO_NAME;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
	return DEVICE_OUT_BLUETOOTH_SCO_HEADSET_NAME;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
	return DEVICE_OUT_BLUETOOTH_SCO_CARKIT_NAME;
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP:
	return DEVICE_OUT_BLUETOOTH_A2DP_NAME;
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES:
	return DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES_NAME;
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER:
	return DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER_NAME;
    case AUDIO_DEVICE_OUT_AUX_DIGITAL:
	return DEVICE_OUT_AUX_DIGITAL_NAME;
    case AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET:
	return DEVICE_OUT_ANLG_DOCK_HEADSET_NAME;
    case AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET:
	return DEVICE_OUT_DGTL_DOCK_HEADSET_NAME;
#if 0 /* not in 4.0.4 */
    case AUDIO_DEVICE_OUT_USB_ACCESSORY:
	return DEVICE_OUT_USB_ACCESSORY_NAME;
    case AUDIO_DEVICE_OUT_USB_DEVICE:
	return DEVICE_OUT_USB_DEVICE_NAME;
#endif
    case AUDIO_DEVICE_IN_DEFAULT:
    default:
	return "";
    }
}


/*
 See notes in the EmergencyDialer.java code
 You really need to check the audioManager.getRingerMode() to see
 if it is RINGER_MODE_SILENT or RINGER_MODE_VIBRATE before playing audio tones.
 */

enum { TONE_RELATIVE_VOLUME = 80 };

class MyListener : public MediaPlayerListener
{
public:
    // Notify messages defined in include/media/mediaplayer.h
    virtual void notify(int msg, int ext1, int ext2, const Parcel *obj) {
	switch (msg) {
	case MEDIA_PLAYBACK_COMPLETE:
	    printf("Playback complete\n");
	    exit(0);
	    break;

	case MEDIA_ERROR:
	    printf("Received Media Error %d %d\n", ext1, ext2);
	    exit(0);

	case MEDIA_INFO:
	    printf("Received media info %d %d\n", ext1, ext2);
	    break;

	default:
	    printf("Not handling MediaPlayerListener message %d %d %d\n", msg, ext1, ext2);
	    break;
	}
    }
};


float getMasterVolume() 
{
    float value = 0;
    if (AudioSystem::getMasterVolume(&value) != NO_ERROR)
	fprintf(stderr, "Failed to get master volume\n");
    return value;
}

void setMasterVolume(float value) 
{
    if (AudioSystem::setMasterVolume(value) != NO_ERROR)
	fprintf(stderr, "Failed to set master volume to %f\n", value);
}

void playTone(ToneGenerator::tone_type tone) 
{
    ToneGenerator *tonegen = new ToneGenerator(AUDIO_STREAM_MUSIC, TONE_RELATIVE_VOLUME);
    tonegen->startTone(tone, 150);   // Run for 150 ms
}
	    
float getStreamVolume(audio_stream_type_t stream, int output) 
{
    float value = 0;
    if (AudioSystem::getStreamVolume(stream, &value, output) != NO_ERROR)
	fprintf(stderr, "Failed to get stream volume\n");
    return value;
}

void setStreamVolume(audio_stream_type_t stream, float volume, int output) 
{
    printf("Setting stream %d to volume %f\n", stream, volume);
    if (AudioSystem::setStreamVolume(stream, volume, output) != NO_ERROR)
	fprintf(stderr, "Failed to set stream volume\n");
}

int getStreamVolumeIndex(audio_stream_type_t stream, audio_devices_t device) 
{
    int value = 0;
    if (AudioSystem::getStreamVolumeIndex(stream, &value
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 40)
#else
                                                        , device
#endif
                                                        ) != NO_ERROR)
	fprintf(stderr, "Failed to get stream volume index\n");
    return value;
}

void setStreamVolumeIndex(audio_stream_type_t stream, int volume, audio_devices_t device)
{
    printf("Setting stream %d to volume index %d\n", stream, volume);
    if (AudioSystem::setStreamVolumeIndex(stream, volume
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 40)
#else
                                                        , device
#endif
                                                        ) != NO_ERROR)
	fprintf(stderr, "Failed to set stream volume index\n");
}

void initStreamVolumeIndex(audio_stream_type_t stream, int value)
{
    printf("Initializing stream %d to max value %d\n", stream, value);
    if (AudioSystem::initStreamVolume(stream, 0, value) != NO_ERROR)
	fprintf(stderr, "Failed to init stream volume index\n");
}

bool getSpeakerPhoneOn()
{
    audio_policy_forced_cfg_t cfg = AudioSystem::getForceUse( AUDIO_POLICY_FORCE_FOR_COMMUNICATION );
    return (cfg == AUDIO_POLICY_FORCE_SPEAKER);
}

void setSpeakerPhoneOn(bool on)
{
    printf("Setting speaker phone to %d\n", on);
    if (AudioSystem::setForceUse( AUDIO_POLICY_FORCE_FOR_COMMUNICATION,
				  (on ? AUDIO_POLICY_FORCE_SPEAKER : AUDIO_POLICY_FORCE_NONE)))
	fprintf(stderr, "Failed to set speaker phone\n");
}

void setDefaults()
{
    printf("Setting system defaults\n");
    
    AudioSystem::setPhoneState(AUDIO_MODE_NORMAL);
//    AudioSystem::setForceUse(FOR_COMMUNICATION, mForcedUseForComm);
//    AudioSystem::setForceUse(FOR_RECORD, mForcedUseForComm);

    for (int i = 0 ; i < AUDIO_STREAM_CNT ; i++) {
	audio_stream_type_t ast = static_cast<audio_stream_type_t>(i);
	if (AudioSystem::initStreamVolume(ast, 0, sMaxStreamVolume[i]) != NO_ERROR)
	    fprintf(stderr, "Failed to init stream %d to max volume index %d\n", i, sMaxStreamVolume[i]);
	if (AudioSystem::setStreamVolumeIndex(ast, sDefaultStreamVolume[i]
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 40)
#else
                                                , AUDIO_DEVICE_OUT_DEFAULT
#endif
                                                                          ) != NO_ERROR)
	    fprintf(stderr, "Failed to set stream %d volume index %d\n", i, sDefaultStreamVolume[i]);
    }
}

// -------------------------------------------------------------------

const char *stream_names[] = { "voice", "system", "ring", "music", "alarm", 
			       "notification", "bluetooth", "enforced", "dtmf", "tts" };

static void usage()
{
    printf("Usage:  audioconfig CMD...\n"
	   "\n"
	   "Commands:   play FILENAME [STREAM]   Play an audio track.  Stream defaults to 'music'\n"
	   "            tone 0-9                 Generate a DTMF tone\n"
	   "            master [FLOAT]           Get or set master volume\n"
	   "            volume [STREAM] [FLOAT]  Get or set stream volume.  If unspecified, show all streams\n"
	   "            index [STREAM] [INT]     Get or set the stream volume index.  If unspecified, show all\n"
	   "            init STREAM MAX          Initialize the index range of STREAM to MAX\n"
	   "            speaker [true|false]     Get or Set the speaker phone to on or off\n"
	   "            setdefaults              Set default configuration\n"
	   "\n"
	   "Stream names can be integers or strings from the following list:\n"
	   "\n"
	);

    for (int i = 0 ; i < AUDIO_STREAM_CNT ; i++) {
	printf(" (%d) %s", i, stream_names[i]);
	if (i < AUDIO_STREAM_CNT - 1) printf(",");
	if ((i+1) % 5 == 0) printf("\n");
    }
    printf("\n");

    exit(0);
}

static bool stringToBool(const char *str)
{
    if (!str) return false;
    return (str[0] == 't' || (str[0] == 'o' && str[1] == 'n') || str[0] == '1');
}

static ToneGenerator::tone_type charToTone(char c)
{
    switch (c) {
    case '0': return ToneGenerator::TONE_DTMF_0;
    case '1': return ToneGenerator::TONE_DTMF_1;
    case '2': return ToneGenerator::TONE_DTMF_2;
    case '3': return ToneGenerator::TONE_DTMF_3;
    case '4': return ToneGenerator::TONE_DTMF_4;
    case '5': return ToneGenerator::TONE_DTMF_5;
    case '6': return ToneGenerator::TONE_DTMF_6;
    case '7': return ToneGenerator::TONE_DTMF_7;
    case '8': return ToneGenerator::TONE_DTMF_8;
    case '9': return ToneGenerator::TONE_DTMF_9;
    case '*': return ToneGenerator::TONE_DTMF_S;
    case '#': return ToneGenerator::TONE_DTMF_P;
    case 'S': return ToneGenerator::TONE_DTMF_S;
    case 'P': return ToneGenerator::TONE_DTMF_P;
    }
    return ToneGenerator::TONE_SUP_DIAL;
}

/*
  /system/core/include/system/audio.h
*/

static audio_stream_type_t streamToInt(const char *stream)
{
    for (int i = 0 ; i < AUDIO_STREAM_CNT ; i++) 
	if (strcasecmp(stream_names[i], stream) == 0)
	    return (audio_stream_type_t) i;
    if (stream[0] < '0' || stream[0] > '9') {
	printf("Invalid stream '%s'\n", stream);
	usage();
    }
    return (audio_stream_type_t) atoi(stream);
}


int main(int argc, char **argv)
{
    bool pending = false;
    if (argc < 2)
	usage();

    ProcessState::self()->startThreadPool();
    android::sp<MediaPlayer> mp;

    if (strcmp(argv[1], "play") == 0) {
	if (argc < 3)
	    usage();
	audio_stream_type_t stream = AUDIO_STREAM_MUSIC;
	if (argc >= 4)
	    stream = streamToInt(argv[3]);

	int fd = ::open(argv[2], O_RDONLY);
	if (fd < 0) {
	    perror("Unable to open file!");
	    exit(1);
	}
	struct stat stat_buf;
	int ret = ::fstat(fd, &stat_buf);
	if (ret < 0) {
	    perror("Unable to stat file");
	    exit(1);
	}
	printf("Setting up file %s, size %lld bytes\n", argv[2], stat_buf.st_size);

	mp = new MediaPlayer();
	mp->setListener(new MyListener);
	mp->setAudioStreamType(stream);
	mp->setDataSource(fd, 0, stat_buf.st_size);
	close(fd);
	mp->prepare();
	mp->start();

	pending = true;
    } 
    else if (strcmp(argv[1], "tone") == 0) {
	if (argc < 3)
	    usage();
	ToneGenerator::tone_type tone = charToTone(argv[2][0]);
	if (tone < 0)
	    fprintf(stderr, "Illegal tone '%s'", argv[2]);
	else 
	    playTone(tone);
	pending = true;
    } else if (strncmp(argv[1], "master", 6) == 0) {
	if (argc < 3) 
	    printf("%f\n", getMasterVolume());
	else {
	    setMasterVolume(atof(argv[2]));
	    printf("%f\n", getMasterVolume());
	}
    } else if (strncmp(argv[1], "vol", 3) == 0) {
	if (argc < 2 || argc > 4)
	    usage();
	if (argc == 2) {
	    for (int i = 0 ; i < AUDIO_STREAM_CNT ; i++) {
		float v = getStreamVolume(static_cast<audio_stream_type_t>(i), 0);
		printf("%i %-12s %f [%3d]\n", i, stream_names[i], v, AudioSystem::logToLinear(v));
	    }
	}
	else {
	    audio_stream_type_t stream = streamToInt(argv[2]);
	    if (argc == 4)
		setStreamVolume(stream, atof(argv[3]), 0);
	    float v = getStreamVolume(stream, 0);
	    printf("%f [%d]\n", v, AudioSystem::logToLinear(v));
	}
    } else if (strncmp(argv[1], "ind", 3) == 0) {
	if (argc < 2 || argc > 4)
	    usage();
	if (argc == 2) {
	    for (int i = 0 ; i < AUDIO_STREAM_CNT ; i++) {
		int v = getStreamVolumeIndex((audio_stream_type_t) i, AUDIO_DEVICE_OUT_DEFAULT);
		printf("%i %-12s %d\n", i, stream_names[i], v);
	    }
	}
	else {
	    audio_stream_type_t stream = streamToInt(argv[2]);
	    if (argc == 4)
		setStreamVolumeIndex(stream, atoi(argv[3]), AUDIO_DEVICE_OUT_DEFAULT);
	    int v = getStreamVolumeIndex(stream, AUDIO_DEVICE_OUT_DEFAULT);
	    printf("%d\n", v);
	}
    } else if (strncmp(argv[1], "ini", 3) == 0) {
	if (argc != 4)
	    usage();
	audio_stream_type_t stream = streamToInt(argv[2]);
	initStreamVolumeIndex(stream, atoi(argv[3]));
    } else if (strncmp(argv[1], "spe", 3) == 0) {
	if (argc != 2 && argc != 3)
	    usage();
	if (argc == 3)
	    setSpeakerPhoneOn(stringToBool(argv[2]));
	printf("%s\n", getSpeakerPhoneOn() ? "on" : "off");
    }  else if (strncmp(argv[1], "set", 3) == 0) {
	setDefaults();
    } else {
	printf("Unexpected command '%s'\n", argv[1]);
	usage();
    }
    
    while (pending) {
	sleep(1);
    }
}
