/*
  Some common string utils
*/

#ifndef _STRING_UTILS_H
#define _STRING_UTILS_H

#include <utils/Vector.h>
#include <utils/String8.h>

namespace android {

Vector<String8> splitString(const char *start, char token, unsigned int max_values = 10000);
Vector<String8> splitString(const String8& string, char token, unsigned int max_values = 10000);
int findString(const String8& data, const char *s, int start_offset = 0);
String8 replaceString(const String8& data, const char *s1, const char *s2);
String8 trimString(const char *data);
String8 trimString(const String8& data);

};  // namespace android

#endif // _STRING_UTILS_H
