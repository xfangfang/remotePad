#include <string.h>

#include "pad.h"

static int32_t dummySetLightBar(RemotePad *pad, OrbisPadColor *inputColor) {
    (void) pad;
    (void) inputColor;
    return 0;
}

static int32_t dummyResetLightBar(RemotePad *pad) {
    (void) pad;
    return 0;
}

static int32_t dummySetVibration(RemotePad *pad, const OrbisPadVibeParam *param) {
    (void) pad;
    (void) param;
    return 0;
}

static int32_t dummyGetControllerInformation(RemotePad *pad, OrbisPadInformation *info) {
    (void) pad;
    emptyPadInfo(info);
    return 0;
}

static int32_t dummyDeviceClassParseData(RemotePad *pad, const OrbisPadData *data, OrbisPadDeviceClassData *classData) {
    (void) pad;
    (void) data;
    memset(classData, 0, sizeof(OrbisPadDeviceClassData));
    classData->deviceClass = ORBIS_PAD_DEVICE_CLASS_PAD;
    classData->bDataValid = true;
    return 0;
}

static int32_t dummyDeviceClassGetExtInfo(RemotePad *pad, OrbisPadDeviceClassExtInfo *info) {
    (void) pad;
    memset(info, 0, sizeof(OrbisPadDeviceClassExtInfo));
    info->deviceClass = ORBIS_PAD_DEVICE_CLASS_PAD;
    return 0;
}

static int32_t dummyRead(RemotePad *pad, OrbisPadData *data, int32_t count) {
    (void) pad;
    (void) count;
    emptyPadData(data);
    return 1;
}

static int32_t dummyReadState(RemotePad *pad, OrbisPadData *data) {
    (void) pad;
    emptyPadData(data);
    return 0;
}

static int32_t dummyClose(RemotePad *pad) {
    pad->userId = 0;
    return 0;
}

int32_t dummyInit(RemotePadDriverPtr driver) {
    (void) driver;
    return 0;
}

int32_t dummyTerm(RemotePadDriverPtr driver) {
    (void) driver;
    return 0;
}

const struct RemotePadDriver dummyDriver = {
        .name = "dummy",
        .init = dummyInit,
        .term = dummyTerm,
        .setLightBar = dummySetLightBar,
        .resetLightBar = dummyResetLightBar,
        .setVibration = dummySetVibration,
        .getControllerInformation = dummyGetControllerInformation,
        .deviceClassParseData = dummyDeviceClassParseData,
        .deviceClassGetExtInfo = dummyDeviceClassGetExtInfo,
        .read = dummyRead,
        .readState = dummyReadState,
        .close = dummyClose,
        .data = NULL
};
