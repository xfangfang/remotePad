#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef REMOTE_PAD_COMMON_DATA_H
#define REMOTE_PAD_COMMON_DATA_H

typedef struct circularBuf {
    uint32_t head;
    uint32_t tail;
    uint32_t dataSize;
    uint32_t maxHistory;

    void (*clearData)(void *);

    char data[];
} circularBuf;

void *getDatePtr(circularBuf *buf, uint32_t index);

int initData(circularBuf **buf, uint32_t dataSize, uint32_t maxHistory, void (*emptyData)(void *));

void termData(circularBuf *buf);

void pushData(circularBuf *buf, const void *data);

void getLatestData(circularBuf *buf, void *data);

int32_t getData(circularBuf *buf, void *data, int32_t count);

#endif //REMOTE_PAD_COMMON_DATA_H
