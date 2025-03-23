#include <stdio.h>
#include <orbis/_types/errors.h>
#include <orbis/libkernel.h>
#include "pad.h"
#include "utils.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

extern RemotePadService rps;

extern const RemotePadDriver dummyDriver;
extern const RemotePadDriver wsDriver;
extern const RemotePadDriver usbDriver;

static const RemotePadDriver *const padDrivers[] = {&dummyDriver, &wsDriver};

#define GET_PAD(handle) \
    RemotePad *pad = NULL; \
    scePthreadMutexLock(&rps.padMutex); \
    int ret = rps.getPad(handle, &pad); \
    if (ret != 0) { \
        scePthreadMutexUnlock(&rps.padMutex); \
        return ret; \
    }

#define CALL_FUNCTION(func, ...) \
    if (pad->driver->func) { \
        ret = pad->driver->func(pad, ## __VA_ARGS__); \
    } else { \
        ret = dummyDriver.func(pad, ## __VA_ARGS__); \
    }

#define RETURN_CODE() \
    scePthreadMutexUnlock(&rps.padMutex); \
    return ret;

#define PAD_FUNC_DEF(...) \
    GET_PAD(handle) \
    CALL_FUNCTION(__VA_ARGS__) \
    RETURN_CODE()

static int32_t init(void) {
    int ret;
    for (size_t i = 0; i < ARRAY_SIZE(padDrivers); i++) {
        if (padDrivers[i]->init(padDrivers[i]) != 0) {
            return -1;
        }
    }

    ret = scePthreadMutexInit(&rps.padMutex, 0, "padMtx");
    if (ret < 0) {
        final_printf("[RemotePad]: failed to init the pad mutex, 0x%X\n", ret);
        return -1;
    }

    for (int i = 0; i < REMOTE_PAD_MAX_PADS; i++) {
        rps.pads[i].index = i;
        rps.pads[i].handle = 1000 + i;
        rps.pads[i].userId = 0;
    }
    return 0;
}

static int32_t term(void) {
    scePthreadMutexLock(&rps.padMutex);
    for (int i = 0; i < REMOTE_PAD_MAX_PADS; i++) {
        if (rps.pads[i].userId != 0) {
            rps.pads[i].driver->close(&rps.pads[i]);
        }
    }
    scePthreadMutexUnlock(&rps.padMutex);
    return 0;
}

static int32_t getPad(int32_t handle, RemotePad **padPtr) {
    RemotePad *pad = NULL;
    if (padPtr == NULL)
        return ORBIS_PAD_ERROR_INVALID_ARG;
    for (int i = 0; i < REMOTE_PAD_MAX_PADS; i++) {
        if (rps.pads[i].handle == handle) {
            pad = &rps.pads[i];
            break;
        }
    }
    if (pad == NULL)
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    else if (pad->userId == 0)
        return ORBIS_HID_ERROR_ALREADY_LOGGED_OUT;
    *padPtr = pad;
    return 0;
}

static int32_t padSetLightBar(int32_t handle, OrbisPadColor *inputColor) {
    PAD_FUNC_DEF(setLightBar, inputColor)
}

static int32_t padResetLightBar(int32_t handle) {
    PAD_FUNC_DEF(resetLightBar)
}

static int32_t padSetVibration(int32_t handle, const OrbisPadVibeParam *param) {
    PAD_FUNC_DEF(setVibration, param)
}

static int32_t padGetControllerInformation(int32_t handle, OrbisPadInformation *info) {
    PAD_FUNC_DEF(getControllerInformation, info)
}

static int32_t padDeviceClassParseData(int32_t handle, const OrbisPadData *data, OrbisPadDeviceClassData *classData) {
    PAD_FUNC_DEF(deviceClassParseData, data, classData)
}

static int32_t padDeviceClassGetExtInfo(int32_t handle, OrbisPadDeviceClassExtInfo *info) {
    PAD_FUNC_DEF(deviceClassGetExtInfo, info)
}

static int32_t padRead(int32_t handle, OrbisPadData *data, int32_t count) {
    PAD_FUNC_DEF(read, data, count)
}

static int32_t padReadState(int32_t handle, OrbisPadData *data) {
    PAD_FUNC_DEF(readState, data)
}

static int32_t padGetHandle(int32_t userId, uint32_t controller_type, uint32_t controller_index) {
    int32_t handle = 0;
    (void)controller_type;
    (void)controller_index;
    scePthreadMutexLock(&rps.padMutex);
    for (int i = 0; i < REMOTE_PAD_MAX_PADS; i++) {
        if (rps.pads[i].userId == userId) {
            handle = rps.pads[i].handle;
            break;
        }
    }
    scePthreadMutexUnlock(&rps.padMutex);
    if (handle != 0)
        return handle;
    return ORBIS_PAD_ERROR_DEVICE_NO_HANDLE;
}

static int32_t padOpen(int32_t userId, int32_t type, int32_t index, void *param) {
    int32_t handle = 0;
    (void)type;
    (void)param;
    if (index < 0 || index >= REMOTE_PAD_MAX_PADS)
        return ORBIS_PAD_ERROR_INVALID_ARG;
    scePthreadMutexLock(&rps.padMutex);
    for (int i = 0; i < REMOTE_PAD_MAX_PADS; i++) {
        if (rps.pads[i].userId == userId) {
            handle = ORBIS_PAD_ERROR_ALREADY_OPENED;
            break;
        }
    }
    if (handle != 0) {
        scePthreadMutexUnlock(&rps.padMutex);
        return handle;
    }

    // TODO: Choose driver based on configuration file
    rps.pads[index].userId = userId;
    rps.pads[index].driver = &wsDriver;
    handle = rps.pads[index].handle;

    scePthreadMutexUnlock(&rps.padMutex);
    if (handle != 0)
        return handle;

    return ORBIS_USER_SERVICE_ERROR_NOT_LOGGED_IN;
}

static int32_t padClose(int32_t handle) {
    PAD_FUNC_DEF(close)
}

RemotePadService rps = {
        .init = init,
        .term = term,
        .getPad = getPad,
        .setLightBar = padSetLightBar,
        .resetLightBar = padResetLightBar,
        .setVibration = padSetVibration,
        .getControllerInformation = padGetControllerInformation,
        .deviceClassParseData = padDeviceClassParseData,
        .deviceClassGetExtInfo = padDeviceClassGetExtInfo,
        .read = padRead,
        .readState = padReadState,
        .getHandle = padGetHandle,
        .open = padOpen,
        .close = padClose,
};

RemotePadService *initRemotePadService(void) {
    rps.init();
    return &rps;
}

void termRemotePadService(RemotePadService *remotePadService) {
    if (remotePadService)
        remotePadService->term();
}

void emptyPadData(OrbisPadData *data) {
    memset(data, 0, sizeof(OrbisPadData));
    data->connected = 1;
    data->count = 1;
    data->leftStick.x = 128;
    data->leftStick.y = 128;
    data->rightStick.x = 128;
    data->rightStick.y = 128;
    data->timestamp = sceKernelGetProcessTime();
}

void emptyPadInfo(OrbisPadInformation *info) {
    memset(info, 0, sizeof(OrbisPadInformation));
    info->connected = 1;
    info->count = 1;
    info->stickDeadzoneL = 0x0d;
    info->stickDeadzoneR = 0x0d;
    info->touchResolutionX = 1920;
    info->touchResolutionY = 942;
    info->touchpadDensity = 44.86f;
}

void circularInit(circularBuf *buf) {
    memset(buf, 0, sizeof(circularBuf));
    // If there is no data at start, this empty data will be returned
    emptyPadData(&buf->data[REMOTE_PAD_MAX_HISTORY - 1]);
}

void circularPush(circularBuf *buf, OrbisPadData *data) {
    memcpy(&buf->data[buf->head], data, sizeof(OrbisPadData));
    buf->head = (buf->head + 1) % REMOTE_PAD_MAX_HISTORY;
    if (buf->head == buf->tail)
        buf->tail = (buf->tail + 1) % REMOTE_PAD_MAX_HISTORY;
}

void circularGetLatest(circularBuf *buf, OrbisPadData *data) {
    uint32_t lastDataIndex = (buf->head + REMOTE_PAD_MAX_HISTORY - 1) % REMOTE_PAD_MAX_HISTORY;
    memcpy(data, &buf->data[lastDataIndex], sizeof(OrbisPadData));
}

int32_t circularGet(circularBuf *buf, OrbisPadData *data, int32_t count) {
    int32_t ret = 0;
    if (count > REMOTE_PAD_MAX_HISTORY)
        count = REMOTE_PAD_MAX_HISTORY;
    if (buf->tail == buf->head) {
        emptyPadData(data);
        return 0;
    }
    while (count > 0 && buf->tail != buf->head) {
        memcpy(data, &buf->data[buf->tail], sizeof(OrbisPadData));
        buf->tail = (buf->tail + 1) % REMOTE_PAD_MAX_HISTORY;
        data++;
        count--;
        ret++;
    }

    return ret;
}
