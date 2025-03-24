#include "common_data.h"

inline void *getDatePtr(circularBuf *buf, uint32_t index) {
    if (index >= buf->maxHistory) {
        return NULL;
    }
    return &buf->data[index * buf->dataSize];
}

int initData(circularBuf **buf, uint32_t dataSize, uint32_t maxHistory, void (*emptyData)(void *)) {
    *buf = malloc(sizeof(circularBuf) + dataSize * maxHistory);
    if (*buf == NULL)
        return -1;
    memset(*buf, 0, sizeof(circularBuf) + dataSize * maxHistory);
    (*buf)->dataSize = dataSize;
    (*buf)->maxHistory = maxHistory;
    (*buf)->emptyData = emptyData;
    // If there is no data at start, this empty data will be returned
    emptyData(getDatePtr(*buf, maxHistory - 1));
    return 0;
}

void termData(circularBuf *buf) {
    if (buf == NULL)
        return;
    free(buf);
}

void pushData(circularBuf *buf, void *data) {
    if (buf == NULL) {
        return;
    }
    memcpy(getDatePtr(buf, buf->head), data, buf->dataSize);
    buf->head = (buf->head + 1) % buf->maxHistory;
    if (buf->head == buf->tail)
        buf->tail = (buf->tail + 1) % buf->maxHistory;
}

void getLatestData(circularBuf *buf, void *data) {
    if (buf == NULL) {
        buf->emptyData(data);
        return;
    }
    uint32_t lastDataIndex = (buf->head + buf->maxHistory - 1) % buf->maxHistory;
    memcpy(data, getDatePtr(buf, lastDataIndex), buf->dataSize);
}

int32_t getData(circularBuf *buf, void *data, int32_t count) {
    int32_t ret = 0;
    if (buf == NULL || buf->tail == buf->head) {
        buf->emptyData(data);
        return 0;
    }
    if (count > buf->maxHistory)
        count = (int32_t) buf->maxHistory;
    while (count > 0 && buf->tail != buf->head) {
        memcpy(data, getDatePtr(buf, buf->tail), buf->dataSize);
        buf->tail = (buf->tail + 1) % buf->maxHistory;
        data++;
        count--;
        ret++;
    }
    return ret;
}
