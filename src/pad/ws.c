#include <string.h>
#include <mongoose.h>
#include <orbis/libkernel.h>
#include <ifaddrs.h>

#include "pad.h"
#include "utils.h"

typedef struct wsDriverData {
    struct mg_mgr mgr;
    struct mg_rpc *rpc_head;
    OrbisPadVibeParam lastVibration[REMOTE_PAD_MAX_PADS];
    OrbisPadColor lastColor[REMOTE_PAD_MAX_PADS];
    bool lightBarReset[REMOTE_PAD_MAX_PADS];
    bool running;
} wsDriverData;

enum wsWakeupEvent {
    WS_WAKEUP_VIBRATION = 0,
    WS_WAKEUP_LIGHTBAR,
    WS_WAKEUP_RESET_LIGHTBAR,
};

union usa {
    struct sockaddr sa;
    struct sockaddr_in sin;
#if MG_ENABLE_IPV6
    struct sockaddr_in6 sin6;
#endif
};

static bool mg_socketpair(MG_SOCKET_TYPE sp[2], union usa usa[2]) {
    socklen_t n = sizeof(usa[0].sin);
    bool success = false;

    sp[0] = sp[1] = MG_INVALID_SOCKET;
    (void) memset(&usa[0], 0, sizeof(usa[0]));
    usa[0].sin.sin_family = AF_INET;
    *(uint32_t *) &usa->sin.sin_addr = mg_htonl(0x7f000001U);  // 127.0.0.1
    usa[1] = usa[0];

    if ((sp[0] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) != MG_INVALID_SOCKET &&
        (sp[1] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) != MG_INVALID_SOCKET &&
        bind(sp[0], &usa[0].sa, n) == 0 &&          //
        bind(sp[1], &usa[1].sa, n) == 0 &&          //
        getsockname(sp[0], &usa[0].sa, &n) == 0 &&  //
        getsockname(sp[1], &usa[1].sa, &n) == 0 &&  //
        connect(sp[0], &usa[1].sa, n) == 0 &&       //
        connect(sp[1], &usa[0].sa, n) == 0) {       //
        success = true;
    }
    if (!success) {
        if (sp[0] != MG_INVALID_SOCKET) close(sp[0]);
        if (sp[1] != MG_INVALID_SOCKET) close(sp[1]);
        sp[0] = sp[1] = MG_INVALID_SOCKET;
    }
    return success;
}

static void tomgaddr(union usa *usa, struct mg_addr *a, bool is_ip6) {
    a->is_ip6 = is_ip6;
    a->port = usa->sin.sin_port;
    memcpy(&a->ip, &usa->sin.sin_addr, sizeof(uint32_t));
#if MG_ENABLE_IPV6
    if (is_ip6) {
    memcpy(a->ip, &usa->sin6.sin6_addr, sizeof(a->ip));
    a->port = usa->sin6.sin6_port;
    a->scope_id = (uint8_t) usa->sin6.sin6_scope_id;
  }
#endif
}

static void ws_wakeup_fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_READ) {
        // MG_INFO(("Got data"));
        // mg_hexdump(c->recv.buf, c->recv.len);
        struct mg_connection *t;
        for (t = c->mgr->conns; t != NULL; t = t->next) {
            if (t->is_websocket) {
                struct mg_str data = mg_str_n((char *) c->recv.buf, c->recv.len);
                mg_call(t, MG_EV_WAKEUP, &data);
            }
        }
        c->recv.len = 0;  // Consume received data
    } else if (ev == MG_EV_CLOSE) {
        close(c->mgr->pipe);         // When we're closing, close the other
        c->mgr->pipe = MG_INVALID_SOCKET;  // side of the socketpair, too
    }
    (void) ev_data;
}

static bool ws_wakeup_init(struct mg_mgr *mgr) {
    bool ok = false;
    if (mgr->pipe == MG_INVALID_SOCKET) {
        union usa usa[2];
        MG_SOCKET_TYPE sp[2] = {MG_INVALID_SOCKET, MG_INVALID_SOCKET};
        struct mg_connection *c = NULL;
        if (!mg_socketpair(sp, usa)) {
            MG_ERROR(("Cannot create socket pair"));
        } else if ((c = mg_wrapfd(mgr, (int) sp[1], ws_wakeup_fn, NULL)) == NULL) {
            close(sp[0]);
            close(sp[1]);
            sp[0] = sp[1] = MG_INVALID_SOCKET;
        } else {
            tomgaddr(&usa[0], &c->rem, false);
            MG_DEBUG(("%lu %p pipe %lu", c->id, c->fd, (unsigned long) sp[0]));
            mgr->pipe = sp[0];
            ok = true;
        }
    }
    return ok;
}

static bool ws_wakeup(struct mg_mgr *mgr, uint8_t type, uint8_t index, const void *buf, size_t len) {
    if (mgr->pipe != MG_INVALID_SOCKET) {
        size_t msg_len = len + sizeof(type) + sizeof(index);
        char *extended_buf = (char *) alloca(msg_len);
        memcpy(extended_buf, &type, sizeof(type));
        memcpy(extended_buf + sizeof(type), &index, sizeof(index));
        memcpy(extended_buf + sizeof(type) + sizeof(index), buf, len);
        send(mgr->pipe, extended_buf, msg_len, 0);
        return true;
    }
    return false;
}

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
    if (getifaddrs(&ifaddr) == -1) {
        final_printf("Failed to get network interfaces\n");
        return;
    }

    // Enumerate all AF_INET IPs
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        if (ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in *in = (struct sockaddr_in *) ifa->ifa_addr;
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

    // Push the data to the pad circular buffer
    pushPadData(index, &padData);
}

static void rpc_notify(struct mg_rpc_req *r) {
    char *text = mg_json_get_str(r->frame, "$.params[0]");
    Notify(TEX_ICON_SYSTEM, text);
    free(text);
}

static void rpc_info(struct mg_rpc_req *r) {
    mg_rpc_ok(r, "{%m:%m}", MG_ESC("version"), MG_ESC(STR(BUILD_TAG_VERSION)));
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_OPEN) {
        // c->is_hexdumping = 1;
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
        wsDriverData *ctx = (wsDriverData *) c->fn_data;
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
        struct mg_iobuf io = {0, 0, 0, 512};
        struct mg_rpc_req r = {&ctx->rpc_head, 0, mg_pfn_iobuf, &io, 0, wm->data};
        mg_rpc_process(&r);
        if (io.buf) mg_ws_send(c, (char *) io.buf, io.len, WEBSOCKET_OP_TEXT);
        mg_iobuf_free(&io);
    } else if (ev == MG_EV_WAKEUP) {
        struct mg_str *data = (struct mg_str *) ev_data;
        uint8_t type = *((uint8_t *) data->buf);
        uint8_t index = *((uint8_t *) (data->buf + 1));
        void *buf = data->buf + 2;
        switch (type) {
            case WS_WAKEUP_VIBRATION: {
                OrbisPadVibeParam *param = (OrbisPadVibeParam *) buf;
                mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:[%d,%d,%d]}",
                             MG_ESC("method"), MG_ESC("v"), MG_ESC("params"),
                             index, param->lgMotor, param->smMotor);
                break;
            }
            case WS_WAKEUP_LIGHTBAR: {
                OrbisPadColor *color = (OrbisPadColor *) buf;
                mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:[%d,%d,%d,%d]}",
                             MG_ESC("method"), MG_ESC("l"), MG_ESC("params"),
                             index, color->r, color->g, color->b);
                break;
            }
            case WS_WAKEUP_RESET_LIGHTBAR:
                mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:[%d]}",
                             MG_ESC("method"), MG_ESC("rl"), MG_ESC("params"),
                             index);
                break;
            default:
                break;
        }
    }
}

void *websocketThread(void *thread_arg) {
    wsDriverData *ctx = (wsDriverData *) thread_arg;
    mg_mgr_init(&ctx->mgr);
    ws_wakeup_init(&ctx->mgr);
    mg_log_set(MG_LL_NONE);

    mg_rpc_add(&ctx->rpc_head, mg_str("u"), rpc_update, ctx);
    mg_rpc_add(&ctx->rpc_head, mg_str("notify"), rpc_notify, ctx);
    mg_rpc_add(&ctx->rpc_head, mg_str("info"), rpc_info, ctx);
    mg_rpc_add(&ctx->rpc_head, mg_str("rpc.list"), mg_rpc_list, &ctx->rpc_head);

    final_printf("Starting WS listener on %s\n", s_listen_on);
    mg_http_listen(&ctx->mgr, s_listen_on, fn, ctx);
    notifyServerAddress();
    while (ctx->running) {
        mg_mgr_poll(&ctx->mgr, 4);
    }
    mg_mgr_free(&ctx->mgr);
    mg_rpc_del(&ctx->rpc_head, NULL);
    ctx->running = false;
    return (void *) 0;
}

static int32_t wsSetLightBar(RemotePad *pad, OrbisPadColor *inputColor) {
    wsDriverData *ctx = pad->driver->data;
    if (!ctx->lightBarReset[pad->index] &&
        memcmp(inputColor, &ctx->lastColor[pad->index], sizeof(OrbisPadColor)) == 0) {
        return 0;
    }
    memcpy(&ctx->lastColor[pad->index], inputColor, sizeof(OrbisPadColor));
    ctx->lightBarReset[pad->index] = false;
    ws_wakeup(&ctx->mgr, WS_WAKEUP_LIGHTBAR, pad->index, inputColor, sizeof(OrbisPadColor));
    return 0;
}

static int32_t wsResetLightBar(RemotePad *pad) {
    wsDriverData *ctx = pad->driver->data;
    if (ctx->lightBarReset[pad->index]) {
        return 0;
    }
    ctx->lightBarReset[pad->index] = true;
    ws_wakeup(&ctx->mgr, WS_WAKEUP_RESET_LIGHTBAR, pad->index, NULL, 0);
    return 0;
}

static int32_t wsSetVibration(RemotePad *pad, const OrbisPadVibeParam *param) {
    wsDriverData *ctx = pad->driver->data;
    if (memcmp(param, &ctx->lastVibration[pad->index], sizeof(OrbisPadVibeParam)) == 0 &&
        param->lgMotor == 0 && param->smMotor == 0) {
        // Some games keep sending empty vibration data. Ignore it to reduce traffic.
        return 0;
    }
    memcpy(&ctx->lastVibration[pad->index], param, sizeof(OrbisPadVibeParam));
    ws_wakeup(&ctx->mgr, WS_WAKEUP_VIBRATION, pad->index, param, sizeof(OrbisPadVibeParam));
    return 0;
}

static int32_t wsInit(RemotePadDriverPtr driver) {
    wsDriverData *ctx = driver->data;
    int ret = 0;
    if (ctx->running) {
        final_printf("[RemotePad]: ws driver is already initialized\n");
        return 0;
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
    if (!ctx)
        return -1;

    if (ctx->running) {
        ctx->running = false;
        scePthreadJoin(serverThread, 0);
    }

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