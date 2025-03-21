#include <stdint.h>
#include <stdbool.h>
#include <orbis/_types/pad.h>
#include <orbis/_types/user.h>
#include <orbis/_types/pthread.h>

#ifndef REMOTE_PAD_PAD_H
#define REMOTE_PAD_PAD_H

#define ORBIS_HID_ERROR_ALREADY_LOGGED_OUT 0x803B0101
#define REMOTE_PAD_MAX_PADS 4
#define REMOTE_PAD_MAX_HISTORY 64

typedef struct circularBuf {
    OrbisPadData data[REMOTE_PAD_MAX_HISTORY];
    uint32_t head;
    uint32_t tail;
} circularBuf;

void emptyPadInfo(OrbisPadInformation *info);

void emptyPadData(OrbisPadData *data);

void circularInit(circularBuf *buf);

void circularPush(circularBuf *buf, OrbisPadData *data);

void circularGetLatest(circularBuf *buf, OrbisPadData *data);

int32_t circularGet(circularBuf *buf, OrbisPadData *data, int32_t count);

typedef struct RemotePad RemotePad;
typedef const struct RemotePadDriver *const RemotePadDriverPtr;
typedef struct RemotePadDriver {
    int32_t (*init)(RemotePadDriverPtr driver);

    int32_t (*term)(RemotePadDriverPtr driver);

    int32_t (*setLightBar)(RemotePad *pad, OrbisPadColor *inputColor);

    int32_t (*resetLightBar)(RemotePad *pad);

    int32_t (*setVibration)(RemotePad *pad, const OrbisPadVibeParam *param);

    int32_t (*getControllerInformation)(RemotePad *pad, OrbisPadInformation *info);

    int32_t (*read)(RemotePad *pad, OrbisPadData *data, int32_t count);

    int32_t (*readState)(RemotePad *pad, OrbisPadData *data);

    int32_t (*close)(RemotePad *pad);

    // Driver specific global data
    void *data;

    const char* name;
} RemotePadDriver;

typedef struct RemotePad {
    int32_t index;
    int32_t handle;
    int32_t userId;

    const RemotePadDriver *driver;
} RemotePad;

typedef struct RemotePadService {
    int32_t (*init)();

    int32_t (*term)();

    int32_t (*getPad)(int32_t handle, RemotePad **padPtr);

    int32_t (*setLightBar)(int32_t handle, OrbisPadColor *inputColor);

    int32_t (*resetLightBar)(int32_t handle);

    int32_t (*setVibration)(int32_t handle, const OrbisPadVibeParam *param);

    int32_t (*getControllerInformation)(int32_t handle, OrbisPadInformation *info);

    int32_t (*read)(int32_t handle, OrbisPadData *data, int32_t count);

    int32_t (*readState)(int32_t handle, OrbisPadData *data);

    int32_t (*getHandle)(int32_t userId, uint32_t controller_type, uint32_t controller_index);

    int32_t (*open)(int32_t userId, int32_t type, int32_t index, void *param);

    int32_t (*close)(int32_t handle);

    RemotePad pads[REMOTE_PAD_MAX_PADS];
    OrbisPthreadMutex padMutex;
} RemotePadService;

RemotePadService *initRemotePadService();

void termRemotePadService(RemotePadService *);

#endif //REMOTE_PAD_PAD_H
