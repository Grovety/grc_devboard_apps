#ifndef _UTILS_H_
#define _UTILS_H_

#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

#endif // _UTILS_H_
