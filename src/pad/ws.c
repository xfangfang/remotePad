#include <string.h>
#include <mongoose.h>
#include <orbis/libkernel.h>
#include <ifaddrs.h>

#include "pad.h"
#include "utils.h"

typedef struct wsDriverData {
    struct mg_mgr mgr;
    struct mg_rpc *rpc_head;
    circularBuf padData[REMOTE_PAD_MAX_PADS];
    OrbisPthreadMutex padMutex;
    bool running;
} wsDriverData;

// Needed by mongoose errno() function
int *attr_public __errno_location(void) {
    int *__error(void);
    return __error();
}

#define REMOTE_PAD_WS_PORT "4263"
static const char *s_listen_on = "ws://0.0.0.0:" REMOTE_PAD_WS_PORT;

static OrbisPthread serverThread;
static wsDriverData globalDriverData = {
        .rpc_head = NULL,
        .running = false
};

static void notifyServerAddress(void) {
    struct ifaddrs *ifaddr;
    char ip[INET_ADDRSTRLEN];
    if(getifaddrs(&ifaddr) == -1) {
        final_printf("Failed to get network interfaces\n");
        return;
    }

    // Enumerate all AF_INET IPs
    for(struct ifaddrs *ifa=ifaddr; ifa!=NULL; ifa=ifa->ifa_next) {
        if(ifa->ifa_addr == NULL) {
            continue;
        }

        if(ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in *in = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &(in->sin_addr), ip, sizeof(ip));

        if (strncmp(ip, "127.0.0.1", sizeof(ip)) == 0) {
            continue;
        }

        Notify(TEX_ICON_SYSTEM, "RemotePad Server:\n %s:" REMOTE_PAD_WS_PORT, ip);
    }
    freeifaddrs(ifaddr);
}

static inline void climp(uint32_t *val, uint32_t min, uint32_t max) {
    if (*val < min) *val = min;
    if (*val > max) *val = max;
}

static void rpc_update(struct mg_rpc_req *r) {
    wsDriverData *ctx = r->rpc->fn_data;
    uint32_t index = mg_json_get_long(r->frame, "$.params[0]", 0);
    uint32_t button = mg_json_get_long(r->frame, "$.params[1]", 0);
    uint32_t leftStickX = mg_json_get_long(r->frame, "$.params[2]", 128);
    uint32_t leftStickY = mg_json_get_long(r->frame, "$.params[3]", 128);
    uint32_t rightStickX = mg_json_get_long(r->frame, "$.params[4]", 128);
    uint32_t rightStickY = mg_json_get_long(r->frame, "$.params[5]", 128);
    uint32_t left2 = mg_json_get_long(r->frame, "$.params[6]", 0);
    uint32_t right2 = mg_json_get_long(r->frame, "$.params[7]", 0);

    climp(&leftStickX, 0, 255);
    climp(&leftStickY, 0, 255);
    climp(&rightStickX, 0, 255);
    climp(&rightStickY, 0, 255);
    climp(&left2, 0, 255);
    climp(&right2, 0, 255);

    if (index >= REMOTE_PAD_MAX_PADS)
        return;

    OrbisPadData padData;
    memset(&padData, 0, sizeof(OrbisPadData));
    padData.buttons = button;
    padData.leftStick.x = leftStickX;
    padData.leftStick.y = leftStickY;
    padData.rightStick.x = rightStickX;
    padData.rightStick.y = rightStickY;
    padData.analogButtons.l2 = left2;
    padData.analogButtons.r2 = right2;
    padData.timestamp = sceKernelGetProcessTime();
    padData.count = 1;
    padData.connected = 1;

    scePthreadMutexLock(&ctx->padMutex);
    circularPush(&ctx->padData[index], &padData);
    scePthreadMutexUnlock(&ctx->padMutex);
}

static void rpc_notify(struct mg_rpc_req *r) {
    char *text = mg_json_get_str(r->frame, "$.params[0]");
    Notify(TEX_ICON_SYSTEM, text);
    free(text);
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_OPEN) {
        // c->is_hexdumping = 1;
    } else if (ev == MG_EV_WS_OPEN) {
        c->data[0] = 'W';  // Mark this connection as an established WS client
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        struct mg_str *connection = mg_http_get_header(hm, "Connection");
        if (connection && mg_match(*connection, mg_str("*Upgrade*"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
            return;
        }
        // Serve static files
        struct mg_http_serve_opts opts = {.root_dir = "/client", .fs = &mg_fs_packed};
        mg_http_serve_dir(c, ev_data, &opts);
    } else if (ev == MG_EV_WS_MSG) {
        // Got websocket frame. Received data is wm->data
        wsDriverData *data = (wsDriverData *) c->fn_data;
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
        struct mg_iobuf io = {0, 0, 0, 512};
        struct mg_rpc_req r = {&data->rpc_head, 0, mg_pfn_iobuf, &io, 0, wm->data};
        mg_rpc_process(&r);
        if (io.buf) mg_ws_send(c, (char *) io.buf, io.len, WEBSOCKET_OP_TEXT);
        mg_iobuf_free(&io);
    }
}

void *websocketThread(void *thread_arg) {
    wsDriverData *ctx = (wsDriverData *) thread_arg;
    mg_mgr_init(&ctx->mgr);
    mg_log_set(MG_LL_NONE);

    mg_rpc_add(&ctx->rpc_head, mg_str("u"), rpc_update, ctx);
    mg_rpc_add(&ctx->rpc_head, mg_str("notify"), rpc_notify, ctx);
    mg_rpc_add(&ctx->rpc_head, mg_str("rpc.list"), mg_rpc_list, &ctx->rpc_head);

    final_printf("Starting WS listener on %s\n", s_listen_on);
    mg_http_listen(&ctx->mgr, s_listen_on, fn, ctx);
    notifyServerAddress();
    while (ctx->running) {
        mg_mgr_poll(&ctx->mgr, 100);
    }
    mg_mgr_free(&ctx->mgr);
    mg_rpc_del(&ctx->rpc_head, NULL);
    ctx->running = false;
    return (void *) 0;
}

static int32_t wsSetLightBar(RemotePad *pad, OrbisPadColor *inputColor) {
    return 0;
}

static int32_t wsResetLightBar(RemotePad *pad) {
    return 0;
}

static int32_t wsSetVibration(RemotePad *pad, const OrbisPadVibeParam *param) {
    wsDriverData *ctx = pad->driver->data;
    struct mg_mgr *mgr = &ctx->mgr;
    // Broadcast message to all connected websocket clients.
    for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) {
        if (c->data[0] != 'W') continue;
        mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:[%d,%d,%d]}",
                     MG_ESC("method"), MG_ESC("vibration"), MG_ESC("params"),
                     pad->index, param->lgMotor, param->smMotor);
    }
    return 0;
}

static int32_t wsRead(RemotePad *pad, OrbisPadData *data, int32_t count) {
    wsDriverData *ctx = pad->driver->data;
    int32_t ret;
    scePthreadMutexLock(&ctx->padMutex);
    ret = circularGet(&ctx->padData[pad->index], data, count);
    scePthreadMutexUnlock(&ctx->padMutex);
    return ret;
}

static int32_t wsReadState(RemotePad *pad, OrbisPadData *data) {
    wsDriverData *ctx = pad->driver->data;
    scePthreadMutexLock(&ctx->padMutex);
    circularGetLatest(&ctx->padData[pad->index], data);
    scePthreadMutexUnlock(&ctx->padMutex);
    return 0;
}

static int32_t wsInit(RemotePadDriverPtr driver) {
    wsDriverData *ctx = driver->data;
    int ret = 0;
    if (ctx->running) {
        final_printf("[RemotePad]: ws driver is already initialized\n");
        return 0;
    }

    ret = scePthreadMutexInit(&ctx->padMutex, 0, "wsSrvMtx");
    if (ret < 0) {
        final_printf("[RemotePad]: failed to init the server mutex, 0x%X\n", ret);
        return -1;
    }

    for (int i = 0; i < REMOTE_PAD_MAX_PADS; i++) {
        circularInit(&ctx->padData[i]);
    }

    ctx->running = true;
    ret = scePthreadCreate(&serverThread, 0, (void *) &websocketThread, ctx, "WsSrvThr");
    if (ret < 0) {
        final_printf("[RemotePad]: Failed to spawn a ws server thread, 0x%X\n", ret);
        return ret;
    }
    return 0;
}

static int32_t wsTerm(RemotePadDriverPtr driver) {
    wsDriverData *ctx = driver->data;
    if (ctx)
        ctx->running = false;

    return 0;
}

const struct RemotePadDriver wsDriver = {
        .name = "websocket",
        .init = wsInit,
        .term = wsTerm,
        .setLightBar = wsSetLightBar,
        .resetLightBar = wsResetLightBar,
        .setVibration = wsSetVibration,
        .data = &globalDriverData
};