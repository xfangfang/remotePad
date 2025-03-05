#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <Patcher.h>

#include "utils.h"
#include "pad.h"
#include "user.h"
#include "config.h"

attr_public const char *g_pluginName = "remote_gamepad";
attr_public const char *g_pluginDesc = "Control your ps4 through network";
attr_public const char *g_pluginAuth = "xfangfang";
attr_public uint32_t g_pluginVersion = 0x00000100; // 1.00

#define PLUGIN_CONFIG_PATH GOLDHEN_PATH "/remote_pad.ini"
#define PLUGIN_DEFAULT_SECTION "default"

static Patcher *scePadReadExtPatcher;
static Patcher *scePadReadStateExtPatcher;
static Patcher *sceUserServiceGetPsnPasswordForDebugPatcher;

static RemoteUserService *remoteUserService;
static RemotePadService *remotePad;

static bool prxLoaded = false;

int32_t scePadReadExt(int32_t, OrbisPadData *, int32_t);

int32_t scePadReadStateExt(int32_t, OrbisPadData *);

int32_t sceUserServiceGetPsnPasswordForDebug(int32_t, char *, size_t);

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
    return scePadReadStateExt(handle, data);
}

HOOK_DEFINE(scePadRead, int32_t handle, OrbisPadData *data, int32_t count) {
    int32_t ret = remotePad->read(handle, data, count);
    if (ret >= 0)
        return ret;
    return scePadReadExt(handle, data, count);
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

HOOK_DEFINE(sceUserServiceGetUserName, int32_t userId, char *username, size_t size) {
    if (remoteUserService->getUserName(userId, username, size) == SCE_OK)
        return SCE_OK;
    return sceUserServiceGetPsnPasswordForDebug(userId, username, size);
}

inline static RemoteUser *findFreeUser(int32_t *index) {
    while(*index < REMOTE_PAD_MAX_USERS) {
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

inline static bool file_exists(const char* filename) {
    struct stat buff;
    return stat(filename, &buff) == 0 ? true : false;
}

int32_t load_config(ini_table_s* table, const char* section_name) {
    char key[16];
    for(int i = 0; i < REMOTE_PAD_MAX_USERS; i++) {
        snprintf(key, sizeof(key), "user%d_enabled", i);
        ini_table_get_entry_as_bool(table, section_name, key, &remoteUserService->users[i].enabled);
        snprintf(key, sizeof(key), "user%d_id", i);
        ini_table_get_entry_as_int(table, section_name, key, &remoteUserService->users[i].userId);
        snprintf(key, sizeof(key), "user%d_name", i);
        const char* name = ini_table_get_entry(table, section_name, key);
        remoteUserService->setUserName(remoteUserService->users[i].userId, name);
    }
    return 0;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[]) {
    final_printf("[GoldHEN] %s Plugin Started.\n", g_pluginName);
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

    // scePadReadExt => scePadRead
    scePadReadExtPatcher = (Patcher *) malloc(sizeof(Patcher));
    Patcher_Construct(scePadReadExtPatcher);
    uint8_t xor_ecx_ecx[5] = {0x31, 0xC9, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(scePadReadExtPatcher, (uint64_t) scePadReadExt, xor_ecx_ecx, sizeof(xor_ecx_ecx));

    // scePadReadStateExt => scePadReadState
    scePadReadStateExtPatcher = (Patcher *) malloc(sizeof(Patcher));
    Patcher_Construct(scePadReadStateExtPatcher);
    uint8_t xor_edx_edx[5] = {0x31, 0xD2, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(scePadReadStateExtPatcher, (uint64_t) scePadReadStateExt, xor_edx_edx, sizeof(xor_edx_edx));

    // sceUserServiceGetPsnPasswordForDebug => sceUserServiceGetUserName
    sceUserServiceGetPsnPasswordForDebugPatcher = (Patcher *) malloc(sizeof(Patcher));
    Patcher_Construct(sceUserServiceGetPsnPasswordForDebugPatcher);
    uint8_t mov_esi_01[5] = {0xBE, 0x01, 0x00, 0x00, 0x00};
    Patcher_Install_Patch(sceUserServiceGetPsnPasswordForDebugPatcher,
                          (uint64_t) sceUserServiceGetPsnPasswordForDebug + 17, mov_esi_01, sizeof(mov_esi_01));

    HOOK32(scePadOpen);
    HOOK32(scePadGetHandle);
    HOOK32(scePadRead);
    HOOK32(scePadReadState);
    HOOK32(scePadGetControllerInformation);
    HOOK32(scePadSetLightBar);
    HOOK32(scePadResetLightBar);
    HOOK32(scePadSetVibration);
    HOOK32(scePadClose);

    HOOK32(sceUserServiceGetLoginUserIdList);
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

    ini_table_s* config = ini_table_create();
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
        ini_section_s* section = &config->section[i];

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

    // If the prx is not loaded correctly, then we exit directly
    if (!prxLoaded)
        return 0;

    termRemoteUserService(remoteUserService);
    termRemotePadService(remotePad);

    UNHOOK(scePadOpen);
    UNHOOK(scePadGetHandle);
    UNHOOK(scePadRead);
    UNHOOK(scePadReadState);
    UNHOOK(scePadGetControllerInformation);
    UNHOOK(scePadSetLightBar);
    UNHOOK(scePadResetLightBar);
    UNHOOK(scePadSetVibration);
    UNHOOK(scePadClose);

    UNHOOK(sceUserServiceGetLoginUserIdList);
    UNHOOK(sceUserServiceGetUserName);
    UNHOOK(sceUserServiceGetEvent);

    Patcher_Destroy(scePadReadExtPatcher);
    Patcher_Destroy(scePadReadStateExtPatcher);
    Patcher_Destroy(sceUserServiceGetPsnPasswordForDebugPatcher);
    free(scePadReadExtPatcher);
    free(scePadReadStateExtPatcher);
    free(sceUserServiceGetPsnPasswordForDebugPatcher);
    return 0;
}

int32_t attr_module_hidden module_start(int64_t argc, const void *args) {
    return 0;
}

int32_t attr_module_hidden module_stop(int64_t argc, const void *args) {
    return 0;
}
