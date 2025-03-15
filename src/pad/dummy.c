#include <string.h>

#include "pad.h"

static int32_t dummySetLightBar(RemotePad *pad, OrbisPadColor *inputColor) {
    return 0;
}

static int32_t dummyResetLightBar(RemotePad *pad) {
    return 0;
}

static int32_t dummySetVibration(RemotePad *pad, const OrbisPadVibeParam *param) {
    return 0;
}

static int32_t dummyGetControllerInformation(RemotePad *pad, OrbisPadInformation *info) {
    emptyPadInfo(info);
    return 0;
}

static int32_t dummyRead(RemotePad *pad, OrbisPadData *data, int32_t count) {
    emptyPadData(data);
    return 1;
}

static int32_t dummyReadState(RemotePad *pad, OrbisPadData *data) {
    emptyPadData(data);
    return 0;
}

static int32_t dummyClose(RemotePad *pad) {
    return 0;
}

int32_t dummyInit(RemotePadDriverPtr driver) {
    return 0;
}

int32_t dummyTerm(RemotePadDriverPtr driver) {
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
        .read = dummyRead,
        .readState = dummyReadState,
        .close = dummyClose,
        .data = NULL
};