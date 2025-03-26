#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "utils.h"
#include "pad.h"
#include "user.h"
#include "config.h"

attr_public const char *g_pluginName = "remote_gamepad";
attr_public const char *g_pluginDesc = "Control your ps4 through network";
attr_public const char *g_pluginAuth = "xfangfang";
attr_public uint32_t g_pluginVersion = 0x00000110; // 1.1.0

#define PLUGIN_CONFIG_PATH GOLDHEN_PATH "/remote_pad.ini"
#define PLUGIN_DEFAULT_SECTION "default"

static RemoteUserService *remoteUserService;
static RemotePadService *remotePad;

static bool prxLoaded = false;

HOOK_DEFINE(scePadInit, void) {
    if (remotePad->initDriver() != SCE_OK) {
        final_printf("Failed to init remote pad driver\n");
    }
    final_printf("RemotePad driver loaded\n");
    return HOOK_PASS(scePadInit);
}

HOOK_DEFINE(scePadOpen, int32_t userId, int32_t type, int32_t index, void *param) {
    RemoteUser *user = remoteUserService->getUser(userId);
    if (user != NULL)
        return remotePad->open(userId, type, user->index, param);

    return HOOK_PASS(scePadOpen, userId, type, index, param);
}

HOOK_DEFINE(scePadClose, int32_t handle) {
    if (remotePad->close(handle) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadClose, handle);
}

HOOK_DEFINE(scePadGetHandle, int32_t userId, uint32_t controller_type, uint32_t controller_index) {
    RemoteUser *user = remoteUserService->getUser(userId);
    if (user != NULL)
        return remotePad->getHandle(userId, controller_type, controller_index);
    return HOOK_PASS(scePadGetHandle, userId, controller_type, controller_index);
}

HOOK_DEFINE(scePadReadState, int32_t handle, OrbisPadData *data) {
    if (remotePad->readState(handle, data) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadReadState, handle, data);
}

HOOK_DEFINE(scePadRead, int32_t handle, OrbisPadData *data, int32_t count) {
    int32_t ret = remotePad->read(handle, data, count);
    if (ret >= 0)
        return ret;
    return HOOK_PASS(scePadRead, handle, data, count);
}

HOOK_DEFINE(scePadGetControllerInformation, int32_t handle, OrbisPadInformation *info) {
    if (remotePad->getControllerInformation(handle, info) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadGetControllerInformation, handle, info);
}

HOOK_DEFINE(scePadSetLightBar, int32_t handle, OrbisPadColor *inputColor) {
    if (remotePad->setLightBar(handle, inputColor) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadSetLightBar, handle, inputColor);
}

HOOK_DEFINE(scePadResetLightBar, int32_t handle) {
    if (remotePad->resetLightBar(handle) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadResetLightBar, handle);
}

HOOK_DEFINE(scePadSetVibration, int32_t handle, const OrbisPadVibeParam *param) {
    if (remotePad->setVibration(handle, param) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadSetVibration, handle, param);
}

HOOK_DEFINE(scePadResetOrientation, int32_t handle) {
    if (remotePad->resetOrientation(handle) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadResetOrientation, handle);
}

HOOK_DEFINE(scePadSetMotionSensorState, int32_t handle, bool enable) {
    if (remotePad->setMotionSensorState(handle, enable) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadSetMotionSensorState, handle, enable);
}

HOOK_DEFINE(scePadSetTiltCorrectionState, int32_t handle, bool enable) {
    if (remotePad->setTiltCorrectionState(handle, enable) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadSetTiltCorrectionState, handle, enable);
}

HOOK_DEFINE(scePadSetAngularVelocityDeadbandState, int32_t handle, bool enable) {
    if (remotePad->setAngularVelocityDeadbandState(handle, enable) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadSetAngularVelocityDeadbandState, handle, enable);
}

HOOK_DEFINE(scePadDeviceClassParseData, int32_t handle, const OrbisPadData *data, OrbisPadDeviceClassData *classData) {
    if (remotePad->deviceClassParseData(handle, data, classData) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadDeviceClassParseData, handle, data, classData);
}

HOOK_DEFINE(scePadDeviceClassGetExtendedInformation, int32_t handle, OrbisPadDeviceClassExtInfo *info) {
    if (remotePad->deviceClassGetExtInfo(handle, info) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(scePadDeviceClassGetExtendedInformation, handle, info);
}

HOOK_DEFINE(sceUserServiceGetUserName, int32_t userId, char *username, size_t size) {
    if (remoteUserService->getUserName(userId, username, size) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(sceUserServiceGetUserName, userId, username, size);
}

HOOK_DEFINE(sceUserServiceGetUserColor, int32_t userId, OrbisUserServiceUserColor *color) {
    if (remoteUserService->getUserColor(userId, color) == SCE_OK)
        return SCE_OK;
    return HOOK_PASS(sceUserServiceGetUserColor, userId, color);
}

inline static RemoteUser *findFreeUser(int32_t *index) {
    while (*index < REMOTE_PAD_MAX_USERS) {
        RemoteUser *user = &remoteUserService->users[*index];
        if (user->enabled && !user->isSystemUserId)
            return user;
        (*index)++;
    }
    return NULL;
}

HOOK_DEFINE(sceUserServiceGetLoginUserIdList, OrbisUserServiceLoginUserIdList *list) {
    int32_t ret = HOOK_PASS(sceUserServiceGetLoginUserIdList, list);
    if (ret < 0)
        return ret;

    for (int i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++) {
        if (list->userId[i] == ORBIS_USER_SERVICE_USER_ID_INVALID)
            continue;

        for (int j = 0; j < REMOTE_PAD_MAX_USERS; j++) {
            RemoteUser *user = remoteUserService->getUserByIndex(j);
            if (user != NULL && user->userId == list->userId[i])
                user->isSystemUserId = true;
        }
    }

    int remoteUserIndex = 0;
    for (int i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++) {
        if (list->userId[i] != ORBIS_USER_SERVICE_USER_ID_INVALID)
            continue;

        // Fill empty position with remote users
        RemoteUser *user = findFreeUser(&remoteUserIndex);
        if (user == NULL) continue;
        list->userId[i] = user->userId;
        remoteUserIndex++;
    }

    // Disable extra remote users
    for (int i = remoteUserIndex; i < REMOTE_PAD_MAX_USERS; i++) {
        RemoteUser *user = remoteUserService->getUserByIndex(remoteUserIndex++);
        if (user != NULL && !user->isSystemUserId)
            user->enabled = false;
    }
    return ret;
}

HOOK_DEFINE(sceUserServiceGetEvent, OrbisUserServiceEvent *event) {
    static int32_t loggedUserCount = 0;
    int ret = HOOK_PASS(sceUserServiceGetEvent, event);
    if (ret == SCE_OK) {
        loggedUserCount += event->event == SCE_USER_SERVICE_EVENT_TYPE_LOGIN ? 1 : -1;
        return ret;
    }

    static bool no_event = false;
    if (!no_event && ret == ORBIS_USER_SERVICE_ERROR_NO_EVENT && loggedUserCount < ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        for (int i = 0; i < REMOTE_PAD_MAX_USERS; i++) {
            RemoteUser *user = remoteUserService->getUserByIndex(i);
            if (user == NULL || user->isLoggedIn || !user->enabled || user->isSystemUserId)
                continue;
            user->isLoggedIn = true;
            event->userId = user->userId;
            event->event = SCE_USER_SERVICE_EVENT_TYPE_LOGIN;
            loggedUserCount++;
            return SCE_OK;
        }
        no_event = true;
    }
    return ret;
}


int32_t load_config(ini_table_s *table, const char *section_name) {
    char key[16];
    for (int i = 0; i < REMOTE_PAD_MAX_USERS; i++) {
        snprintf(key, sizeof(key), "user%d_enabled", i);
        ini_table_get_entry_as_bool(table, section_name, key, &remoteUserService->users[i].enabled);
        snprintf(key, sizeof(key), "user%d_id", i);
        ini_table_get_entry_as_int(table, section_name, key, &remoteUserService->users[i].userId);
        snprintf(key, sizeof(key), "user%d_name", i);
        const char *name = ini_table_get_entry(table, section_name, key);
        remoteUserService->setUserName(remoteUserService->users[i].userId, name);
    }
    return 0;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[]) {
    final_printf("[GoldHEN] %s Plugin Started.\n", g_pluginName);
    final_printf("[GoldHEN] Git version: %s\n", STR(BUILD_TAG_VERSION));
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    final_printf("[GoldHEN] Plugin Author(s): %s\n", g_pluginAuth);

    if (load_prx("libScePad.sprx", true))
        return -1;

    if (load_prx("libSceUserService.sprx", false))
        return -1;

    prxLoaded = true;

    remoteUserService = initRemoteUserService();
    if (!remoteUserService) {
        final_printf("Failed to init remote user service\n");
        return -1;
    }

    remotePad = initRemotePadService();
    if (!remotePad) {
        final_printf("Failed to init remote pad service\n");
        return -1;
    }

    HOOK32(scePadInit);
    HOOK32(scePadOpen);
    HOOK32(scePadGetHandle);
    HOOK32(scePadRead);
    HOOK32(scePadReadState);
    HOOK32(scePadGetControllerInformation);
    HOOK32(scePadSetLightBar);
    HOOK32(scePadResetLightBar);
    HOOK32(scePadSetVibration);
    HOOK32(scePadResetOrientation);
    HOOK32(scePadSetMotionSensorState);
    HOOK32(scePadSetTiltCorrectionState);
    HOOK32(scePadSetAngularVelocityDeadbandState);
    HOOK32(scePadDeviceClassParseData);
    HOOK32(scePadDeviceClassGetExtendedInformation);
    HOOK32(scePadClose);

    HOOK32(sceUserServiceGetLoginUserIdList);
    HOOK32(sceUserServiceGetUserColor);
    HOOK32(sceUserServiceGetUserName);
    HOOK32(sceUserServiceGetEvent);

    // load config
    struct proc_info procInfo;
    if (sys_sdk_proc_info(&procInfo) == 0) {
        print_proc_info();
    } else {
        final_printf("failed to initialise\n");
        return -1;
    }

    if (!file_exists(PLUGIN_CONFIG_PATH)) {
        final_printf("Not found config file at: \"%s\"\n", PLUGIN_CONFIG_PATH);
        return 0;
    }

    ini_table_s *config = ini_table_create();
    if (config == NULL) {
        final_printf("Config parser failed to initialise\n");
        return -1;
    }

    if (!ini_table_read_from_file(config, PLUGIN_CONFIG_PATH)) {
        final_printf("Config parser failed to parse config: %s\n", PLUGIN_CONFIG_PATH);
        return -1;
    }

    final_printf("Section is TitleID [%s]\n", procInfo.titleid);
    for (int i = 0; i < config->size; i++) {
        ini_section_s *section = &config->section[i];

        if (section == NULL) continue;

        if (strcmp(section->name, PLUGIN_DEFAULT_SECTION) == 0) {
            final_printf("Section [%s] is default\n", section->name);
            load_config(config, section->name);
        } else if (strcmp(section->name, procInfo.titleid) == 0) {
            final_printf("Section is TitleID [%s]\n", procInfo.titleid);
            load_config(config, section->name);
        }
    }

    ini_table_destroy(config);

    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    final_printf("[GoldHEN] %s Plugin Unloaded.\n", g_pluginName);
    Notify(TEX_ICON_SYSTEM, "RemotePad Plugin Unloaded");

    // If the prx is not loaded correctly, then we exit directly
    if (!prxLoaded)
        return 0;

    termRemoteUserService(remoteUserService);
    termRemotePadService(remotePad);

    UNHOOK(scePadInit);
    UNHOOK(scePadOpen);
    UNHOOK(scePadGetHandle);
    UNHOOK(scePadRead);
    UNHOOK(scePadReadState);
    UNHOOK(scePadGetControllerInformation);
    UNHOOK(scePadSetLightBar);
    UNHOOK(scePadResetLightBar);
    UNHOOK(scePadSetVibration);
    UNHOOK(scePadResetOrientation);
    UNHOOK(scePadSetMotionSensorState);
    UNHOOK(scePadSetTiltCorrectionState);
    UNHOOK(scePadSetAngularVelocityDeadbandState);
    UNHOOK(scePadDeviceClassParseData);
    UNHOOK(scePadDeviceClassGetExtendedInformation);
    UNHOOK(scePadClose);

    UNHOOK(sceUserServiceGetLoginUserIdList);
    UNHOOK(sceUserServiceGetUserColor);
    UNHOOK(sceUserServiceGetUserName);
    UNHOOK(sceUserServiceGetEvent);
    return 0;
}

int32_t attr_module_hidden module_start(int64_t argc, const void *args) {
    return 0;
}

int32_t attr_module_hidden module_stop(int64_t argc, const void *args) {
    return 0;
}
