#include <stdio.h>
#include "SigynService.h"

// Contains the UID of the radio process
#include <private/android_filesystem_config.h>

using namespace android;

void usage()
{
    printf("sigynd - Presents a binder interface to the RIL daemon\n"
	   "\n"
	   "The sigynd process needs to be able to open the /dev/socket/rild.\n"
	   "The easiest way is to run as root or give it membership in the\n"
	   "radio (1001) group\n"
	   "\n"
	   "Setting the DEBUG_SIGYN environment variable to 'in', 'out' or 'in:out'\n"
	   "will display extra information about messages over the RIL socket\n"
	);
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc > 1)
	usage();

    printf("Process uid=%d gid=%d\n", getuid(), getgid());
    if (getgid() != AID_RADIO && getuid() != 0) {
	if (setgid(AID_RADIO)) {
	    perror("Unable to set process GID to radio");
	    return 1;
	}
    }

    sp<ProcessState> proc(ProcessState::self());
    SigynService::instantiate();

    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();

    return 0;
}
