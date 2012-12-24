#include "WifiService.h"

#include <stdio.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>


using namespace android;

void usage()
{
    printf("wifi - Presents a binder interface to Wifi\n"
	   "\n"
	);
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc > 1)
	usage();

    // The instantiate function from BinderService.h conflicts with the Singleton function
    // WifiService::instantiate();
    // So we need to do the "publishAndJoinThreadPool()" function by hand

    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(WifiService::getServiceName()), &WifiService::getInstance());
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();

    return 0;
}
