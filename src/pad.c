#include <stdio.h>
#include <orbis/_types/errors.h>
#include <orbis/libkernel.h>
#include "pad.h"
#include "utils.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

extern const RemotePadDriver dummyDriver;
extern const RemotePadDriver wsDriver;
extern const RemotePadDriver usbDriver;

static const RemotePadDriver *const padDrivers[] = {&dummyDriver, &wsDriver};

static int32_t init();

static int32_t term();

static int32_t getPad(int32_t handle, RemotePad **padPtr);

static int32_t padSetLightBar(int32_t handle, OrbisPadColor *inputColor);

static int32_t padResetLightBar(int32_t handle);

static int32_t padSetVibration(int32_t handle, const OrbisPadVibeParam *param);

static int32_t padGetControllerInformation(int32_t handle, OrbisPadInformation *info);

static int32_t padRead(int32_t handle, OrbisPadData *data, int32_t count);

static int32_t padReadState(int32_t handle, OrbisPadData *data);

static int32_t padGetHandle(int32_t userId, uint32_t controller_type, uint32_t controller_index);

static int32_t padOpen(int32_t userId, int32_t type, int32_t index, void *param);

static int32_t padClose(int32_t handle);

#define GET_PAD(handle) NULL; \
    int ret = rps.getPad(handle, &pad); \
    if (ret != 0) \
        return ret;

RemotePadService rps = {
        .init = init,
        .term = term,
        .getPad = getPad,
        .setLightBar = padSetLightBar,
        .resetLightBar = padResetLightBar,
        .setVibration = padSetVibration,
        .getControllerInformation = padGetControllerInformation,
        .read = padRead,
        .readState = padReadState,
        .getHandle = padGetHandle,
        .open = padOpen,
        .close = padClose,
};

static int32_t init() {
    int ret;
    for (int i = 0; i < ARRAY_SIZE(padDrivers); i++) {
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

static int32_t term() {
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
    scePthreadMutexLock(&rps.padMutex);
    for (int i = 0; i < REMOTE_PAD_MAX_PADS; i++) {
        if (rps.pads[i].handle == handle) {
            pad = &rps.pads[i];
            break;
        }
    }
    scePthreadMutexUnlock(&rps.padMutex);
    if (pad == NULL)
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    else if (pad->userId == 0)
        return ORBIS_HID_ERROR_ALREADY_LOGGED_OUT;
    *padPtr = pad;
    return 0;
}

static int32_t padSetLightBar(int32_t handle, OrbisPadColor *inputColor) {
    RemotePad *pad = GET_PAD(handle);
    return pad->driver->setLightBar(pad, inputColor);
}

static int32_t padResetLightBar(int32_t handle) {
    RemotePad *pad = GET_PAD(handle);
    return pad->driver->resetLightBar(pad);
}

static int32_t padSetVibration(int32_t handle, const OrbisPadVibeParam *param) {
    RemotePad *pad = GET_PAD(handle);
    return pad->driver->setVibration(pad, param);
}

static int32_t padGetControllerInformation(int32_t handle, OrbisPadInformation *info) {
    RemotePad *pad = GET_PAD(handle);
    return pad->driver->getControllerInformation(pad, info);
}

static int32_t padRead(int32_t handle, OrbisPadData *data, int32_t count) {
    RemotePad *pad = GET_PAD(handle);
    return pad->driver->read(pad, data, count);
}

static int32_t padReadState(int32_t handle, OrbisPadData *data) {
    RemotePad *pad = GET_PAD(handle);
    return pad->driver->readState(pad, data);
}

static int32_t padGetHandle(int32_t userId, uint32_t controller_type, uint32_t controller_index) {
    int32_t handle = 0;
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

    // TODO: 根据配置文件选择驱动
    rps.pads[index].userId = userId;
    rps.pads[index].driver = &wsDriver;
    handle = rps.pads[index].handle;

    scePthreadMutexUnlock(&rps.padMutex);
    if (handle != 0)
        return handle;

    return ORBIS_USER_SERVICE_ERROR_NOT_LOGGED_IN;
}

static int32_t padClose(int32_t handle) {
    RemotePad *pad = GET_PAD(handle);
    pad->userId = 0;
    return pad->driver->close(pad);
}

RemotePadService *initRemotePadService() {
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