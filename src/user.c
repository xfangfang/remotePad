#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <orbis/_types/errors.h>

#include "user.h"
#include "config.h"

static int32_t init(void);

static int32_t term(void);

static RemoteUser *getUser(int32_t);

static RemoteUser *getUserByIndex(int32_t);

static int32_t getUserName(int32_t userId, char *username, size_t size);

static int32_t setUserName(int32_t userId, const char *username);

static int32_t getUserColor(int32_t userId, OrbisUserServiceUserColor *color);

static RemoteUserService rus = {
        .init = init,
        .term = term,
        .getUser = getUser,
        .getUserByIndex = getUserByIndex,
        .getUserName = getUserName,
        .setUserName = setUserName,
        .getUserColor = getUserColor,
};

static int32_t init(void) {
    for (int i = 0; i < REMOTE_PAD_MAX_USERS; i++) {
        rus.users[i].index = i;
        rus.users[i].userId = 0x20000000 + i; // Guest user id
        rus.users[i].isLoggedIn = false;
        rus.users[i].isSystemUserId = false;
        rus.users[i].enabled = true;
        snprintf(rus.users[i].userName, ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH, "Remote%d", i);
    }

    return 0;
}

static int32_t term(void) {
    return 0;
}

static RemoteUser *getUser(int32_t userId) {
    for (int i = 0; i < REMOTE_PAD_MAX_USERS; i++) {
        if (rus.users[i].userId == userId) {
            return &rus.users[i];
        }
    }
    return NULL;
}

static RemoteUser *getUserByIndex(int32_t index) {
    if (index >= REMOTE_PAD_MAX_USERS || index < 0)
        return NULL;
    return &rus.users[index];
}

static int32_t getUserName(int32_t userId, char *username, size_t size) {
    RemoteUser *user = rus.getUser(userId);
    if (user == NULL)
        return ORBIS_USER_SERVICE_ERROR_NOT_LOGGED_IN;

    if (size < strlen(user->userName) + 1)
        return ORBIS_USER_SERVICE_ERROR_BUFFER_TOO_SHORT;

    strcpy(username, user->userName);

    return 0;
}

static int32_t setUserName(int32_t userId, const char *username) {
    RemoteUser *user = rus.getUser(userId);
    if (user == NULL)
        return ORBIS_USER_SERVICE_ERROR_NOT_LOGGED_IN;

    if (username == NULL)
        return ORBIS_USER_SERVICE_ERROR_INVALID_ARGUMENT;

    size_t len = strlen(username);
    if (len < 3 || len > 16) {
        return ORBIS_USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }

    strcpy(user->userName, username);

    return 0;
}

static int32_t getUserColor(int32_t userId, OrbisUserServiceUserColor *color) {
    RemoteUser *user = rus.getUser(userId);
    if (user == NULL)
        return ORBIS_USER_SERVICE_ERROR_NOT_LOGGED_IN;

    *color = (OrbisUserServiceUserColor) (userId % 4);

    return 0;
}

RemoteUserService *initRemoteUserService(void) {
    rus.init();
    return &rus;
}

void termRemoteUserService(RemoteUserService *remoteUserService) {
    if (remoteUserService)
        remoteUserService->term();
}
