// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <cmath>
#include <ie_blob.h>
#include <legacy/ie_layers_property.hpp>
#include <ie_precision.hpp>
#include <precision_utils.h>
#include <gtest/gtest.h>
#include "single_layer_common.hpp"
#include <math.h>

using namespace InferenceEngine;

void GenRandomDataCommon(Blob::Ptr blob) {
    if (blob->getTensorDesc().getPrecision() == Precision::U8) {
        auto * blobRawDataU8 = blob->buffer().as<uint8_t*>();
        size_t count = blob->size();
        for (size_t i = 0; i < count; i++) {
            auto val = static_cast<uint8_t>(rand() % 256);
            blobRawDataU8[i] = val;
        }
    } else if (blob->getTensorDesc().getPrecision() == Precision::FP16) {
        float scale = 2.0f / static_cast<float>(RAND_MAX);
        /* fill by random data in the range (-1, 1)*/
        auto * blobRawDataFp16 = blob->buffer().as<ie_fp16 *>();
        size_t count = blob->size();
        for (size_t indx = 0; indx < count; ++indx) {
            float val = rand();
            val = val * scale - 1.0f;
            blobRawDataFp16[indx] = PrecisionUtils::f32tof16(val);
        }
    } else if (blob->getTensorDesc().getPrecision() == Precision::FP32) {
        float scale = 2.0f / static_cast<float>(RAND_MAX);
        /* fill by random data in the range (-1, 1)*/
        auto * blobRawDataFp32 = blob->buffer().as<float*>();
        size_t count = blob->size();
        for (size_t i = 0; i < count; i++) {
            float val = rand();
            val = val * scale - 1.0f;
            blobRawDataFp32[i] = val;
        }
    } else if (blob->getTensorDesc().getPrecision() == Precision::I32) {
        using T = PrecisionTrait<Precision::I32>::value_type;
        auto buffer = blob->buffer().as<T*>();
        double tempSum = 0;
        int val;
        for (size_t i = 0; i < blob->size(); i++) {
            if ((tempSum > -RAND_MAX) && (tempSum < RAND_MAX))
                val = rand() % (RAND_MAX - static_cast<int>(tempSum));
            else
                val = 0;
            buffer[i] = val;
            tempSum +=val;
        }
    } else {
        IE_THROW() << blob->getTensorDesc().getPrecision() << " is not supported by GenRandomDataCommon";
    }
}

BufferWrapper::BufferWrapper(const Blob::Ptr& blob) : BufferWrapper(blob, blob->getTensorDesc().getPrecision()) {}

BufferWrapper::BufferWrapper(const Blob::Ptr& blob, Precision _precision) : precision(_precision) {
    if (precision == Precision::FP16) {
        fp16_ptr = blob->buffer().as<ie_fp16*>();
    } else if (precision == Precision::FP32) {
        fp32_ptr = blob->buffer().as<float*>();
    } else if (precision == Precision::I32) {
        i32_ptr = blob->buffer().as<int32_t*>();
    } else if (precision == Precision::U8) {
        u8_ptr = blob->buffer().as<uint8_t*>();
    } else {
        IE_THROW() << "Unsupported precision for compare: " << precision;
    }
}

float BufferWrapper::operator[](size_t index) {
    if (precision == Precision::FP16) {
        return PrecisionUtils::f16tof32(fp16_ptr[index]);
    } else if (precision == Precision::I32) {
        return i32_ptr[index];
    } else if (precision == Precision::U8) {
        return u8_ptr[index];
    }
    return fp32_ptr[index];
}

void BufferWrapper::insert(size_t index, float value) {
    if (precision == Precision::FP16) {
        fp16_ptr[index] = PrecisionUtils::f32tof16(value);
    } else if (precision == Precision::I32) {
        i32_ptr[index] = value;
    } else if (precision == Precision::U8) {
        u8_ptr[index] = value;
    } else {
        fp32_ptr[index] = value;
    }
}

void CompareCommonExact(const InferenceEngine::Blob::Ptr &actual,
                        const InferenceEngine::Blob::Ptr &expected) {
    ASSERT_EQ(actual == nullptr, expected == nullptr);
    if (actual == nullptr && expected == nullptr) {
        return;
    }

    ASSERT_EQ(actual->getTensorDesc().getPrecision(), expected->getTensorDesc().getPrecision())
        << "actual is " << actual->getTensorDesc().getPrecision() << ", while reference is " << expected->getTensorDesc().getPrecision();
    ASSERT_EQ(actual->size(), expected->size()) << "actual has " << actual->size() << " elements, while reference " << expected->size();

    auto actualPtr   = actual->cbuffer().as<const std::uint8_t*>();
    auto expectedPtr = expected->cbuffer().as<const std::uint8_t*>();
    for (std::size_t i = 0; i < actual->byteSize(); ++i) {
        ASSERT_EQ(actualPtr[i], expectedPtr[i]) << "first error index = " << i / actual->element_size();
    }
}

void CompareCommonAbsolute(const Blob::Ptr& actual, const Blob::Ptr& expected, float tolerance) {
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);

    BufferWrapper res_ptr(actual);
    BufferWrapper ref_ptr(expected);
    float max_abs_error = 0;
    size_t actualMaxErrId = 0;
    size_t expectedMaxErrId = 0;
    std::function<void(size_t, size_t)> absoluteErrorUpdater = [&](size_t actualIdx, size_t expectedIdx) {
        auto actual = res_ptr[actualIdx];
        auto expected = ref_ptr[expectedIdx];
        float abs_error = fabsf(actual - expected);
        if (abs_error > max_abs_error) {
            max_abs_error = abs_error;
            actualMaxErrId = actualIdx;
            expectedMaxErrId = expectedIdx;
        }
    };
    CompareCommon(actual, expected, absoluteErrorUpdater);

    ASSERT_NEAR(ref_ptr[expectedMaxErrId], res_ptr[actualMaxErrId], tolerance)
                        << "expectedMaxErrId = " << expectedMaxErrId
                        << " actualMaxErrId = " << actualMaxErrId;
}

void CompareCommonRelative(const Blob::Ptr& actual, const Blob::Ptr& expected, float tolerance) {
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);

    BufferWrapper res_ptr(actual);
    BufferWrapper ref_ptr(expected);
    float max_rel_error = 0;
    size_t actualMaxErrId = 0;
    size_t expectedMaxErrId = 0;
    std::function<void(size_t, size_t)> relatedErrorUpdater = [&](size_t actualIdx, size_t expectedIdx) {
        auto actual = res_ptr[actualIdx];
        auto expected = ref_ptr[expectedIdx];
        float abs_error = fabsf(actual - expected);
        float rel_error = expected != 0.0 ? fabsf(abs_error / expected) : abs_error;
        if (rel_error > max_rel_error) {
            max_rel_error = rel_error;
            actualMaxErrId = actualIdx;
            expectedMaxErrId = expectedIdx;
        }
    };
    CompareCommon(actual, expected, relatedErrorUpdater);

    float abs_threshold = fabsf(ref_ptr[expectedMaxErrId]) * tolerance;
    ASSERT_NEAR(ref_ptr[expectedMaxErrId], res_ptr[actualMaxErrId], abs_threshold)
                        << "expectedMaxErrId = " << expectedMaxErrId
                        << " actualMaxErrId = " << actualMaxErrId;
}

// Compare:
// - relative if large result
// - absolute if small result
//
// Justification:
// - If result's absolute value if small (close to 0),
//   it probably is the difference of similar values,
//   so result's leading digits may suffer cancellation.
//   Thus, relative error may be large if small result,
//   while result is correctly close to zero.
void CompareCommonCombined(const Blob::Ptr& actual, const Blob::Ptr& expected, float tolerance) {
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);

    BufferWrapper res_ptr(actual);
    BufferWrapper ref_ptr(expected);
    float max_combi_error = 0;
    size_t actualMaxErrId = 0;
    size_t expectedMaxErrId = 0;
    std::function<void(size_t, size_t)> combinedErrorUpdater = [&](size_t actualIdx, size_t expectedIdx) {
        auto actual = res_ptr[actualIdx];
        auto expected = ref_ptr[expectedIdx];
        float abs_error = fabsf(actual - expected);
        float rel_error = expected != 0.0 ? fabsf(abs_error / expected) : abs_error;
        float error = std::max(abs_error, rel_error);
        if (max_combi_error < error) {
            max_combi_error = error;
            actualMaxErrId = actualIdx;
            expectedMaxErrId = expectedIdx;
        }
    };
    CompareCommon(actual, expected, combinedErrorUpdater);

    float abs_threshold = fabsf(ref_ptr[expectedMaxErrId]) * tolerance;
    ASSERT_NEAR(ref_ptr[expectedMaxErrId], res_ptr[actualMaxErrId], abs_threshold)
                        << "expectedMaxErrId = " << expectedMaxErrId
                        << " actualMaxErrId = " << actualMaxErrId;
}

void CompareCommonWithNorm(const InferenceEngine::Blob::Ptr& actual,
                           const InferenceEngine::Blob::Ptr& expected,
                           float maxDiff) {
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    const uint16_t *res_ptr = actual->buffer().as<const uint16_t*>();
    size_t res_size = actual->size();

    const uint16_t *ref_ptr = expected->buffer().as<const uint16_t*>();
    size_t ref_size = expected->size();

    ASSERT_EQ(res_size, ref_size);

    for (size_t i = 0; i < ref_size; i++) {
        float val_res = PrecisionUtils::f16tof32(res_ptr[i]);
        float val_ref = PrecisionUtils::f16tof32(ref_ptr[i]);
        float norm = std::max(fabs(val_res), fabs(val_ref));
        if (norm < 1.0f)
            norm = 1.0f;
        ASSERT_NEAR( val_res , val_ref, (maxDiff * norm));
    }
}

void CompareCommon(const Blob::Ptr& actual, const Blob::Ptr& expected,
                   const std::function<void(size_t, size_t)>& errorUpdater) {
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);

    Layout res_layout = actual->getTensorDesc().getLayout();
    Layout ref_layout = expected->getTensorDesc().getLayout();
    SizeVector res_dims = actual->getTensorDesc().getDims();

    size_t res_size = actual->size();
    size_t ref_size = expected->size();
    ASSERT_EQ(res_size, ref_size);

    if (res_layout == NCHW || res_layout == NHWC) {
        size_t N = res_dims[0];
        size_t C = res_dims[1];
        size_t H = res_dims[2];
        size_t W = res_dims[3];

        for (size_t n = 0; n < N; n++) {
            for (size_t c = 0; c < C; c++) {
                for (size_t h = 0; h < H; h++) {
                    for (size_t w = 0; w < W; w++) {
                        size_t actualIdx = res_layout == NCHW ?
                                           w + h * W + c * W * H + n * W * H * C : c + w * C + h * C * W +
                                                                                   n * W * H * C;
                        size_t expectedIdx = ref_layout == NCHW ?
                                             w + h * W + c * W * H + n * W * H * C : c + w * C + h * C * W +
                                                                                     n * C * W * H;
                        errorUpdater(actualIdx, expectedIdx);
                    }
                }
            }
        }
    } else {
        if (res_layout == NC) {

            size_t N = res_dims[0];
            size_t C = res_dims[1];
            for (size_t n = 0; n < N; n++) {
                for (size_t c = 0; c < C; c++) {
                    size_t actualIdx =   c +  n * C;
                    errorUpdater(actualIdx, actualIdx);
                }
            }
        } else {
            for (size_t i = 0; i < ref_size; i++) {
                errorUpdater(i, i);
            }
        }
    }
}

void fill_data_common(BufferWrapper& data, size_t size, size_t duty_ratio) {
    for (size_t i = 0; i < size; i++) {
        if ((i / duty_ratio) % 2 == 1) {
            data.insert(i, 0.0);
        } else {
            data.insert(i, sin((float) i));
        }
    }
}
