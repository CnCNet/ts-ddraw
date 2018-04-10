#ifndef _SCALE_PATTERN_H
#define _SCALE_PATTERN_H

typedef enum {
    REPEAT,
    SEQUENCE,
    ONCE,
    END
} SCALE_PATTERN_TYPE;

typedef struct {
    SCALE_PATTERN_TYPE type;
    int dst_index;
    int src_index;
    int count;
} scale_pattern;

#endif
