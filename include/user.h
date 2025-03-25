#include <stdint.h>
#include <stdbool.h>
#include <orbis/_types/user.h>

#ifndef REMOTE_PAD_USER_H
#define REMOTE_PAD_USER_H

#define REMOTE_PAD_MAX_USERS 4

typedef struct RemoteUser {
    int32_t index;
    int32_t userId;
    char userName[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH + 1];
    bool enabled;
    bool isSystemUserId;

    /// private
    // whether user login event has been returned
    bool isLoggedIn;
} RemoteUser;

typedef struct RemoteUserService {
    int32_t (*init)(void);

    int32_t (*term)(void);

    RemoteUser *(*getUser)(int32_t userId);

    RemoteUser *(*getUserByIndex)(int32_t index);

    int32_t (*getUserName)(int32_t userId, char *username, size_t size);

    int32_t (*setUserName)(int32_t userId, const char *username);

    int32_t (*getUserColor)(int32_t userId, OrbisUserServiceUserColor *color);

    RemoteUser users[REMOTE_PAD_MAX_USERS];
} RemoteUserService;

RemoteUserService *initRemoteUserService(void);

void termRemoteUserService(RemoteUserService *);

#endif //REMOTE_PAD_USER_H
