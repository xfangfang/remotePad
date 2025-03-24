#include <stdint.h>
#include <stdbool.h>
#include <orbis/_types/pad.h>
#include <orbis/_types/user.h>
#include <orbis/_types/pthread.h>

#include "common_data.h"

#ifndef REMOTE_PAD_PAD_H
#define REMOTE_PAD_PAD_H

#define ORBIS_HID_ERROR_ALREADY_LOGGED_OUT 0x803B0101
#define REMOTE_PAD_MAX_PADS 4
#define REMOTE_PAD_MAX_HISTORY 64

typedef struct {
    int32_t deviceClass;
    uint8_t reserved[4];
    uint8_t classData[12];
} OrbisPadDeviceClassExtInfo;

typedef struct {
    int32_t deviceClass;
    bool bDataValid;
    uint8_t classData[16];
} OrbisPadDeviceClassData;

void emptyPadInfo(OrbisPadInformation *info);

void emptyPadData(OrbisPadData *data);

void initPadData(size_t index);

void pushPadData(size_t index, OrbisPadData *data);

void getLatestPadData(size_t index, OrbisPadData *data);

int32_t getPadData(size_t index, OrbisPadData *data, int32_t count);

typedef struct RemotePad RemotePad;
typedef const struct RemotePadDriver *RemotePadDriverPtr;
typedef struct RemotePadDriver {
    int32_t (*init)(RemotePadDriverPtr driver);

    int32_t (*term)(RemotePadDriverPtr driver);

    int32_t (*setLightBar)(RemotePad *pad, OrbisPadColor *inputColor);

    int32_t (*resetLightBar)(RemotePad *pad);

    int32_t (*setVibration)(RemotePad *pad, const OrbisPadVibeParam *param);

    int32_t (*getControllerInformation)(RemotePad *pad, OrbisPadInformation *info);

    int32_t (*deviceClassParseData)(RemotePad *pad, const OrbisPadData *data, OrbisPadDeviceClassData *classData);

    int32_t (*deviceClassGetExtInfo)(RemotePad *pad, OrbisPadDeviceClassExtInfo *info);

    int32_t (*read)(RemotePad *pad, OrbisPadData *data, int32_t count);

    int32_t (*readState)(RemotePad *pad, OrbisPadData *data);

    int32_t (*close)(RemotePad *pad);

    // Driver specific global data
    void *data;

    const char *name;
} RemotePadDriver;

typedef struct RemotePad {
    int32_t index;
    int32_t handle;
    int32_t userId;

    const RemotePadDriver *driver;
    circularBuf *padData;
} RemotePad;

typedef struct RemotePadService {
    int32_t (*init)(void);

    int32_t (*term)(void);

    int32_t (*getPad)(int32_t handle, RemotePad **padPtr);

    int32_t (*setLightBar)(int32_t handle, OrbisPadColor *inputColor);

    int32_t (*resetLightBar)(int32_t handle);

    int32_t (*setVibration)(int32_t handle, const OrbisPadVibeParam *param);

    int32_t (*getControllerInformation)(int32_t handle, OrbisPadInformation *info);

    int32_t (*deviceClassParseData)(int32_t handle, const OrbisPadData *data, OrbisPadDeviceClassData *classData);

    int32_t (*deviceClassGetExtInfo)(int32_t handle, OrbisPadDeviceClassExtInfo *info);

    int32_t (*read)(int32_t handle, OrbisPadData *data, int32_t count);

    int32_t (*readState)(int32_t handle, OrbisPadData *data);

    int32_t (*getHandle)(int32_t userId, uint32_t controller_type, uint32_t controller_index);

    int32_t (*open)(int32_t userId, int32_t type, int32_t index, void *param);

    int32_t (*close)(int32_t handle);

    RemotePad pads[REMOTE_PAD_MAX_PADS];
    OrbisPthreadMutex padMutex;
    OrbisPthreadMutex dataMutex;
} RemotePadService;

RemotePadService *initRemotePadService(void);

void termRemotePadService(RemotePadService *);

#endif //REMOTE_PAD_PAD_H
