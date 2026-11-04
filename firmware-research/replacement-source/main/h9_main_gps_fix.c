#include "h9_main_gps_fix.h"

typedef struct {
    const uint8_t *data;
    size_t length;
} Field;

static int decimal(uint8_t value)
{
    return value >= '0' && value <= '9' ? (int)(value - '0') : -1;
}

static int hex_digit(uint8_t value)
{
    if (value >= '0' && value <= '9') return (int)(value - '0');
    if (value >= 'A' && value <= 'F') return (int)(value - 'A') + 10;
    if (value >= 'a' && value <= 'f') return (int)(value - 'a') + 10;
    return -1;
}

static bool two_digits(const uint8_t *data, uint8_t *value)
{
    int tens = decimal(data[0]);
    int ones = decimal(data[1]);
    if (tens < 0 || ones < 0) return false;
    *value = (uint8_t)(tens * 10 + ones);
    return true;
}

static bool time_field(Field field, H9MainGpsFix *fix)
{
    if (field.length < 6u || !two_digits(field.data, &fix->hour) ||
        !two_digits(field.data + 2, &fix->minute) ||
        !two_digits(field.data + 4, &fix->second) ||
        fix->hour > 23u || fix->minute > 59u || fix->second > 60u)
        return false;
    if (field.length > 6u) {
        size_t index;
        if (field.data[6] != '.' || field.length == 7u) return false;
        for (index = 7u; index < field.length; ++index)
            if (decimal(field.data[index]) < 0) return false;
    }
    fix->has_time = true;
    return true;
}

static bool date_field(Field field, H9MainGpsFix *fix)
{
    uint8_t year;
    if (field.length != 6u || !two_digits(field.data, &fix->day) ||
        !two_digits(field.data + 2, &fix->month) ||
        !two_digits(field.data + 4, &year) ||
        fix->day < 1u || fix->day > 31u ||
        fix->month < 1u || fix->month > 12u)
        return false;
    fix->year = (uint16_t)(2000u + year);
    fix->has_date = true;
    return true;
}

static bool coordinate(Field value, Field hemisphere, unsigned degree_digits,
                       int32_t *output)
{
    uint32_t degrees = 0u;
    uint32_t minute_micro;
    uint32_t fraction = 0u;
    uint32_t scale = 100000u;
    size_t index;
    int digit;
    uint8_t direction;
    if (value.length < degree_digits + 2u || hemisphere.length != 1u)
        return false;
    direction = hemisphere.data[0];
    if ((degree_digits == 2u && direction != 'N' && direction != 'S') ||
        (degree_digits == 3u && direction != 'E' && direction != 'W'))
        return false;
    for (index = 0u; index < degree_digits; ++index) {
        digit = decimal(value.data[index]);
        if (digit < 0) return false;
        degrees = degrees * 10u + (uint32_t)digit;
    }
    digit = decimal(value.data[degree_digits]);
    if (digit < 0) return false;
    minute_micro = (uint32_t)digit * 10u;
    digit = decimal(value.data[degree_digits + 1u]);
    if (digit < 0) return false;
    minute_micro = (minute_micro + (uint32_t)digit) * 1000000u;
    if (minute_micro >= 60000000u ||
        (degree_digits == 2u && degrees > 90u) ||
        (degree_digits == 3u && degrees > 180u))
        return false;
    if (value.length > degree_digits + 2u) {
        if (value.data[degree_digits + 2u] != '.' ||
            value.length == degree_digits + 3u)
            return false;
        for (index = degree_digits + 3u; index < value.length; ++index) {
            digit = decimal(value.data[index]);
            if (digit < 0) return false;
            if (scale != 0u) {
                fraction += (uint32_t)digit * scale;
                scale /= 10u;
            }
        }
    }
    minute_micro += fraction;
    if ((degree_digits == 2u && degrees == 90u && minute_micro != 0u) ||
        (degree_digits == 3u && degrees == 180u && minute_micro != 0u))
        return false;
    *output = (int32_t)(degrees * 10000000u + (minute_micro + 3u) / 6u);
    if (direction == 'S' || direction == 'W') *output = -*output;
    return true;
}

H9MainGpsSentenceType h9_main_gps_parse_fix(const uint8_t *sentence,
                                             size_t length,
                                             H9MainGpsFix *fix)
{
    Field fields[20];
    H9MainGpsFix next;
    H9MainGpsSentenceType type;
    size_t end, star, field_count = 0u, start, index;
    uint8_t checksum = 0u;
    int high, low;
    if (sentence == NULL || fix == NULL || length < 10u ||
        length > 255u || sentence[0] != '$' ||
        sentence[length - 1u] != '\n')
        return H9_MAIN_GPS_UNRECOGNIZED;
    end = length - 1u;
    if (sentence[end - 1u] == '\r') --end;
    if (end < 9u) return H9_MAIN_GPS_UNRECOGNIZED;
    star = end - 3u;
    if (sentence[star] != '*') return H9_MAIN_GPS_UNRECOGNIZED;
    high = hex_digit(sentence[star + 1u]);
    low = hex_digit(sentence[star + 2u]);
    if (high < 0 || low < 0) return H9_MAIN_GPS_UNRECOGNIZED;
    for (index = 1u; index < star; ++index) checksum ^= sentence[index];
    if (checksum != (uint8_t)(high * 16 + low))
        return H9_MAIN_GPS_UNRECOGNIZED;
    start = 1u;
    for (index = 1u; index <= star; ++index) {
        if (index != star && sentence[index] != ',') continue;
        if (field_count == sizeof fields / sizeof fields[0])
            return H9_MAIN_GPS_UNRECOGNIZED;
        fields[field_count].data = sentence + start;
        fields[field_count].length = index - start;
        ++field_count;
        start = index + 1u;
    }
    if (fields[0].length != 5u ||
        fields[0].data[0] < 'A' || fields[0].data[0] > 'Z' ||
        fields[0].data[1] < 'A' || fields[0].data[1] > 'Z')
        return H9_MAIN_GPS_UNRECOGNIZED;
    if (fields[0].data[2] == 'R' && fields[0].data[3] == 'M' &&
        fields[0].data[4] == 'C')
        type = H9_MAIN_GPS_RMC;
    else if (fields[0].data[2] == 'G' && fields[0].data[3] == 'G' &&
             fields[0].data[4] == 'A')
        type = H9_MAIN_GPS_GGA;
    else return H9_MAIN_GPS_UNRECOGNIZED;
    next = *fix;
    if (field_count < (type == H9_MAIN_GPS_RMC ? 10u : 7u) ||
        !time_field(fields[1], &next))
        return H9_MAIN_GPS_UNRECOGNIZED;
    if (type == H9_MAIN_GPS_RMC) {
        if (fields[2].length != 1u ||
            (fields[2].data[0] != 'A' && fields[2].data[0] != 'V') ||
            !date_field(fields[9], &next))
            return H9_MAIN_GPS_UNRECOGNIZED;
        next.has_position = fields[2].data[0] == 'A';
        if (next.has_position &&
            (!coordinate(fields[3], fields[4], 2u, &next.latitude_e7) ||
             !coordinate(fields[5], fields[6], 3u, &next.longitude_e7)))
            return H9_MAIN_GPS_UNRECOGNIZED;
    } else {
        if (fields[6].length != 1u || decimal(fields[6].data[0]) < 0)
            return H9_MAIN_GPS_UNRECOGNIZED;
        next.has_position = fields[6].data[0] != '0';
        if (next.has_position &&
            (!coordinate(fields[2], fields[3], 2u, &next.latitude_e7) ||
             !coordinate(fields[4], fields[5], 3u, &next.longitude_e7)))
            return H9_MAIN_GPS_UNRECOGNIZED;
    }
    if (!next.has_position) {
        next.latitude_e7 = 0;
        next.longitude_e7 = 0;
    }
    *fix = next;
    return type;
}
