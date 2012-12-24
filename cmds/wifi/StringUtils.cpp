#include "ctype.h"
#include "StringUtils.h"

namespace android {

Vector<String8> splitString(const char *start, char token, unsigned int max_values)
{
    Vector<String8> result;
    if (*start) {
	const char *ptr = start;
	while (result.size() < max_values - 1) {
	    while (*ptr != token && *ptr)
		ptr++;
	    result.push(String8(start, ptr-start));
	    if (!*ptr)
		break;
	    start = ++ptr;
	}
	if (*ptr)
	    result.push(String8(ptr));
    }
    return result;
}

// Find the first offset of s in data and return it.  Return -1 if not found
// We generally assume s is at least two characters long (otherwise, use the
// faster function in String8 directly
int findString(const String8& data, const char *s, int start_offset)
{
    int n = strlen(s);
    if (n < 2)
	return -1;

    int offset = start_offset;
    const char *p = data.string() + offset;
    while (*p) {
	if (*p == *s && !strncmp(p+1, s+1, n-1))
	    return offset;
	p++;
	offset++;
    }
    return -1;
}

// Replace string s1 with string s2
String8 replaceString(const String8& data, const char *s1, const char *s2)
{
    String8 result;
    int offset, start = 0, n1 = strlen(s1);
    while ((offset = findString(data, s1, start)) >= 0) {
	result.append(data.string() + start, offset - start);
	result.append(s2);
	start += offset + n1;
    }
    result.append(data.string() + start);
    return result;
}

String8 trimString(const char *data)
{
    const char *start = data;

    while (isspace(*start))
	start++;
    const char *end = strlen(start) + start;
    while (end > start && isspace(*(end - 1)))
	end--;
    return String8(start, end-start);
}

}; // namespace android
