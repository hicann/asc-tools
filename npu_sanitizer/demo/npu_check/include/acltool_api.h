#ifndef ACLSAN_ACLTOOL_API_H
#define ACLSAN_ACLTOOL_API_H

#include "aclsan/aclsan_api.h"

#include <stdint.h>

#define ACLSAN_API __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

ACLSAN_API AclsanStatus acltoolInitialize(void);

#ifdef __cplusplus
}
#endif

#endif // ACLSAN_ACLTOOL_API_H
