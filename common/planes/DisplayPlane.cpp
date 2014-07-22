/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <HwcTrace.h>
#include <Hwcomposer.h>
#include <DisplayPlane.h>
#include <GraphicBuffer.h>

namespace android {
namespace intel {

DisplayPlane::DisplayPlane(int index, int type, int disp)
    : mIndex(index),
      mType(type),
      mDevice(disp),
      mInitialized(false),
      mDataBuffers(),
      mIsProtectedBuffer(false),
      mTransform(PLANE_TRANSFORM_0)
{
    CTRACE();

    mPosition.x = 0;
    mPosition.y = 0;
    mPosition.w = 0;
    mPosition.h = 0;

    mSrcCrop.x = 0;
    mSrcCrop.y = 0;
    mSrcCrop.w = 0;
    mSrcCrop.h = 0;
}

DisplayPlane::~DisplayPlane()
{
    CTRACE();
    deinitialize();
}

bool DisplayPlane::initialize(uint32_t bufferCount)
{
    CTRACE();

    // create buffer cache
    mDataBuffers.setCapacity(bufferCount);

    mInitialized = true;
    return true;
}

void DisplayPlane::deinitialize()
{
    // invalid buffer cache
    invalidateBufferCache();

    mInitialized = false;
}

void DisplayPlane::setPosition(int x, int y, int w, int h)
{
    ATRACE("Position = %d, %d - %dx%d", x, y, w, h);


    mPosition.x = x;
    mPosition.y = y;
    mPosition.w = w;
    mPosition.h = h;
}

void DisplayPlane::setSourceCrop(int x, int y, int w, int h)
{
    ATRACE("Source crop = %d, %d - %dx%d", x, y, w, h);

    mSrcCrop.x = x;
    mSrcCrop.y = y;
    mSrcCrop.w = w;
    mSrcCrop.h = h;
}

void DisplayPlane::setTransform(int trans)
{
    ATRACE("transform = %d", trans);

    switch (trans) {
    case PLANE_TRANSFORM_90:
    case PLANE_TRANSFORM_180:
    case PLANE_TRANSFORM_270:
        mTransform = trans;
        break;
    default:
        mTransform = PLANE_TRANSFORM_0;
    }
}

bool DisplayPlane::setDataBuffer(uint32_t handle)
{
    DataBuffer *buffer;
    BufferMapper *mapper;
    ssize_t index;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();

    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("handle = %#x", handle);

    if (!handle) {
        ETRACE("invalid buffer handle");
        return false;
    }

    if (!bm) {
        ETRACE("failed to get buffer manager");
        return false;
    }

    buffer = bm->get(handle);
    if (!buffer) {
        ETRACE("failed to get buffer");
        return false;
    }

    // update buffer's source crop
    buffer->setCrop(mSrcCrop.x, mSrcCrop.y, mSrcCrop.w, mSrcCrop.h);

    mIsProtectedBuffer = GraphicBuffer::isProtectedBuffer((GraphicBuffer*)buffer);
    // map buffer if it's not in cache
    index = mDataBuffers.indexOfKey(buffer->getKey());
    if (index < 0) {
        VTRACE("unmapped buffer, mapping...");
        mapper = bm->map(*buffer);
        if (!mapper) {
            ETRACE("failed to map buffer");
            goto mapper_err;
        }

        // add it to data buffers
        index = mDataBuffers.add(buffer->getKey(), mapper);
        if (index < 0) {
            ETRACE("failed to add mapper");
            goto add_err;
        }
    } else {
        VTRACE("got mapper in saved data buffers");
        mapper = mDataBuffers.valueAt(index);
    }

    // put buffer after getting mapper
    bm->put(*buffer);

    return setDataBuffer(*mapper);
add_err:
    bm->unmap(*mapper);
mapper_err:
    bm->put(*buffer);
    return false;
}

void DisplayPlane::invalidateBufferCache()
{
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    BufferMapper* mapper;

    RETURN_VOID_IF_NOT_INIT();

    if (!bm) {
        ETRACE("failed to get buffer manager");
        return;
    }

    for (size_t i = 0; i < mDataBuffers.size(); i++) {
        mapper = mDataBuffers.valueAt(i);
        // unmap it
        if (mapper)
            bm->unmap(*mapper);
    }

    // clear recorded data buffers
    mDataBuffers.clear();
}

bool DisplayPlane::assignToDevice(int disp)
{
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("disp = %d", disp);

    mDevice = disp;
    return true;
}

} // namespace intel
} // namespace android
