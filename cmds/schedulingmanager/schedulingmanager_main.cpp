/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.

  Act as an Android permission service, which allows anyone to 
  make connections to various services.
 */

#include <binder/BinderService.h>
#include <ISchedulingPolicyService.h>
#include <utils/Singleton.h>
#include <utils/String16.h>

using namespace android;

class SchedulingPolicyService : public BinderService<SchedulingPolicyService>,
    public BnSchedulingPolicyService, public Singleton<SchedulingPolicyService>
{
    friend class BinderService<SchedulingPolicyService>;

public:
    static const char *getServiceName() { return "Scheduling"; }
    // BnSchedulingPolicyService
    int requestPriority(/*pid_t*/int32_t pid, /*pid_t*/int32_t tid, int32_t prio) {
        printf("Setting priority for pid=%d tid=%d prio=%d\n", pid, tid, prio);
        return true;
    }
};

int main(int argc, char **argv)
{
    sp<ProcessState> proc(ProcessState::self());
    SchedulingPolicyService::instantiate();
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
