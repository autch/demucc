/* -*- mode: c; encoding: utf-8-unix; -*- */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

struct mmlbuf* mmlbuf_new()
{
    struct mmlbuf* mmlbuf;

    mmlbuf = calloc(1, sizeof(struct mmlbuf));
    mmlbuf->size = MMLBUF_SIZE;
    mmlbuf->buf = calloc(1, mmlbuf->size);
    mmlbuf->mml = mmlbuf->buf;

    return mmlbuf;
}

void mmlbuf_free(struct mmlbuf* mmlbuf)
{
    if(mmlbuf == NULL) return;
    free(mmlbuf->buf);
    free(mmlbuf);
}

// サイズはそのまま中身をリセット
void mmlbuf_reset(struct mmlbuf* mmlbuf)
{
    mmlbuf->mml = mmlbuf->buf;
    memset(mmlbuf->buf, 0, mmlbuf->size);
}

char* mmlbuf_buf(struct mmlbuf* mmlbuf)
{
    return mmlbuf->buf;
}

size_t mmlbuf_size(struct mmlbuf* mmlbuf)
{
    return mmlbuf->size;
}

size_t mmlbuf_pos(struct mmlbuf* mmlbuf)
{
    return mmlbuf->mml - mmlbuf->buf;
}

size_t mmlbuf_left(struct mmlbuf* mmlbuf)
{
    return mmlbuf_size(mmlbuf) - mmlbuf_pos(mmlbuf);
}

// mmlbuf に、少なくとも space バイト分の空きを確保する
// 空きが十分にあれば、何もしないこともある
static int mmlbuf_require(struct mmlbuf* mmlbuf, int space)
{
    long pos = mmlbuf->mml - mmlbuf->buf;
    long left = mmlbuf->size - pos;
    char* new_buf;
    size_t new_size = mmlbuf->size;

    if(left > space) {
        return 0; // no extension required
    }

    new_size += (MMLBUF_SIZE < space) ? space : MMLBUF_SIZE;
    new_buf = realloc(mmlbuf->buf, new_size);
    if(new_buf == NULL) {
        return -1; // fail
    }
    mmlbuf->size = new_size;
    mmlbuf->buf = new_buf;
    mmlbuf->mml = mmlbuf->buf + pos;
    return 1; // extend happened
}

int mmlbuf_appendv(struct mmlbuf* mmlbuf, char* format, va_list ap)
{
    va_list aq;
    int ret;

    va_copy(aq, ap);
    ret = vsnprintf(mmlbuf->mml, mmlbuf_left(mmlbuf), format, aq);
    va_end(aq);
    if(ret < 0) {
        // error
        return ret;
    } else if(ret < mmlbuf_left(mmlbuf)) {
        // success
        mmlbuf->mml += ret;
        return ret;
    }

    if(mmlbuf_require(mmlbuf, ret) < 0) {
        return -1;
    }

    va_copy(aq, ap);
    ret = vsnprintf(mmlbuf->mml, mmlbuf_left(mmlbuf), format, aq);
    va_end(aq);

    return ret;
}

int mmlbuf_append(struct mmlbuf* mmlbuf, char* format, ...)
{
    int ret;
    va_list ap;
    va_start(ap, format);
    ret = mmlbuf_appendv(mmlbuf, format, ap);
    va_end(ap);
    return ret;
}

