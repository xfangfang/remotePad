#include "utils.h"
#include <orbis/libkernel.h>


int32_t load_prx(const char *name, bool syscall) {
    int32_t ret;
    char module[256];
    snprintf(module, 256, "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), name);
    //TODO: what's the difference ?
    if (syscall) {
        sys_dynlib_load_prx(module, &ret);
    } else {
        ret = sceKernelLoadStartModule(module, 0, 0, 0, 0, 0);
    }
    final_printf("%s prx id = %d\n", name, ret);
    if (ret < 0) {
        Notify(TEX_ICON_SYSTEM, "[RemotePad]\n %s load failed 0x%X", name, ret);
        return 1;
    }
    return 0;
}

// Taken from https://github.com/Al-Azif/ps4-notifi

void Notify(const char *p_Uri, const char *p_Format, ...) {
    OrbisNotificationRequest s_Request;
    memset(&s_Request, '\0', sizeof(s_Request));

    s_Request.reqId = NotificationRequest;
    s_Request.unk3 = 0;
    s_Request.useIconImageUri = 1;
    s_Request.targetId = -1;

    // Maximum size to move is destination size - 1 to allow for null terminator
    if (p_Uri != NULL && strnlen(p_Uri, sizeof(s_Request.iconUri)) + 1 < sizeof(s_Request.iconUri)) {
        strncpy(s_Request.iconUri, p_Uri, strnlen(p_Uri, sizeof(s_Request.iconUri) - 1));
    } else {
        s_Request.useIconImageUri = 0;
    }

    va_list p_Args;
    va_start(p_Args, p_Format);
    // p_Format is controlled externally, some compiler/linter options will mark this as a security issue
    vsnprintf(s_Request.message, sizeof(s_Request.message), p_Format, p_Args);
    va_end(p_Args);

    sceKernelSendNotificationRequest(NotificationRequest, &s_Request, sizeof(s_Request), 0);
}
