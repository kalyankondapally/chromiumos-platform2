/*
 * Copyright (C) 2014-2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _COMMON_UTIL_MACROS_H_
#define _COMMON_UTIL_MACROS_H_

#include <algorithm>
#include "LogHelper.h"
#include <string>

/**
 * Use to check input parameter and if failed, return err_code and print error message
 */
#define VOID_VALUE
#define CheckError(condition, err_code, err_msg, args...) \
            do { \
                if (condition) { \
                    LOGE(err_msg, ##args);\
                    return err_code;\
                }\
            } while (0)

/**
 * Use to check input parameter and if failed, call back error, return err_code and print error message
 */
#define CheckAndCallbackError(condition, err_cb, err_code, err_msg, args...) \
            do { \
                if (condition) { \
                    if (err_cb) { \
                        err_cb->deviceError(); \
                    } \
                    LOGE(err_msg, ##args); \
                    return err_code; \
                } \
            } while (0)

/**
 * Use to check input parameter and if failed, return err_code and print warning message,
 * this should be used for non-vital error checking.
 */
#define CheckWarning(condition, err_code, err_msg, args...) \
            do { \
                if (condition) { \
                    LOGW(err_msg, ##args);\
                    return err_code;\
                }\
            } while (0)

// macro for memcpy
#ifndef MEMCPY_S
#define MEMCPY_S(dest, dmax, src, smax) memcpy((dest), (src), std::min((size_t)(dmax), (size_t)(smax)))
#endif

#define STDCOPY(dst, src, size) std::copy((src), ((src) + (size)), (dst))

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define CLEAR_N(x, n) memset (&(x), 0, sizeof(x) * n)

#define STRLEN_S(x) std::char_traits<char>::length(x)

#define PROPERTY_VALUE_MAX  92

#include <execinfo.h> // backtrace()

/**
 * \macro PRINT_BACKTRACE_LINUX
 * Debug macro for printing backtraces in Linux (GCC compatible) environments
 */
#define PRINT_BACKTRACE_LINUX() \
{ \
    char **btStrings = nullptr; \
    int btSize = 0; \
    void *btArray[10]; \
\
    btSize = backtrace(btArray, 10); \
    btStrings = backtrace_symbols(btArray, btSize); \
\
    LOGE("----------------------------------------"); \
    LOGE("-------------- backtrace ---------------"); \
    LOGE("----------------------------------------"); \
    for (int i = 0; i < btSize; i++) \
        LOGE("%s\n", btStrings[i]); \
    LOGE("----------------------------------------"); \
\
    free(btStrings); \
    btStrings = nullptr; \
    btSize = 0; \
}

/*
 * For Android, from N release (7.0) onwards the folder where the HAL has
 * permissions to dump files is /data/misc/cameraserver/
 * before that was /data/misc/media/
 * This is due to the introduction of the new cameraserver process.
 * For Linux, the same folder is used.
 */
#define CAMERA_OPERATION_FOLDER "/tmp/"

/**
 * \macro UNUSED
 *  applied to parameters not used in a method in order to avoid the compiler
 *  warning
 */
#define UNUSED(x) (void)(x)

#define UNUSED1(a)                                                  (void)(a)
#define UNUSED2(a,b)                                                (void)(a),UNUSED1(b)

#endif /* _COMMON_UTIL_MACROS_H_ */
