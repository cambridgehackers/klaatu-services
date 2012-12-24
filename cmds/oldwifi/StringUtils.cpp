#include "StringUtils.h"

namespace android {

Vector<String8> splitString(const char *start, char token, unsigned int max_values)
{
    Vector<String8> result;
    if (*start) {
	const char *ptr = start;
	while (result.size() < max_values - 1) {
	    while (*ptr != token && *ptr != '\0')
		ptr++;
	    result.push(String8(start, ptr-start));
	    if (*ptr == '\0')
		break;
	    ptr++;
	    start = ptr;
	}
	if (*ptr != '\0')
	    result.push(String8(ptr));
    }
    return result;
}

Vector<String8> splitString(const String8& string, char token, unsigned int max_values)
{
    return splitString(string.string(), token, max_values);
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
	if (*p == *s && strncmp(p+1, s+1, n-1) == 0)
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
    int start = 0;
    int n1 = strlen(s1);
    while (1) {
	int offset = findString(data, s1, start);
	if (offset < 0)
	    break;
	result.append(data.string() + start, offset - start);
	result.append(s2);
	start += offset + n1;
    }
    result.append(data.string() + start);
    return result;
}

static bool isspace(char a)
{
    return (a == ' ' || a == '\t' ||
	    a == '\f' || a == '\r' ||
	    a == '\v' || a == '\n');
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

String8 trimString(const String8& data)
{
    return trimString(data.string());
}

}; // namespace android
