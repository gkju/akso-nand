#ifndef UTILS_H
#define UTILS_H
#include <stdbool.h>
#include <stdlib.h>
#include "nand.h"

typedef enum {
    NONE,
    BOOL,
    NAND
} connection_type;

typedef union {
    const bool *bool_ptr;
    nand_t *nand_ptr;
} connection_value_ptr;

struct connection {
    connection_type type;
    connection_value_ptr value_ptr;
    struct connection *other_end;
    size_t index;
};

typedef struct connection connection;

typedef struct {
    connection *buffer;
    size_t size;
    size_t capacity;
} connection_vector;

static inline ssize_t max(ssize_t a, ssize_t b) {
    return a > b ? a : b;
}

static inline void rectify_other_end(connection_vector *vector, size_t i) {
    if (vector->buffer[i].type == NAND) {
        vector->buffer[i].other_end->other_end = &vector->buffer[i];
    }
}

static inline connection_vector *create_connection_vector() {
    connection_vector *vector = (connection_vector*) malloc(sizeof(connection_vector));
    if (!vector) {
        return NULL;
    }
    vector->buffer = (connection*) malloc(sizeof(connection) * 2);
    vector->size = 0;
    vector->capacity = 2;
    if (!vector->buffer) {
        free(vector);
        return NULL;
    }
    return vector;
}

static inline connection* add_connection(connection_vector *vector) {
    if (vector->size >= vector->capacity) {
        connection *new_buffer = (connection*) realloc(vector->buffer, sizeof(connection) * vector->capacity << 1);
        if (!new_buffer) {
            return NULL;
        }
        vector->capacity <<= 1;
        vector->buffer = new_buffer;

        for (size_t i = 0; i < vector->size; i++) {
            rectify_other_end(vector, i);
        }
    }

    connection *conn = &vector->buffer[vector->size++];
    conn->type = NONE;
    conn->index = vector->size - 1;
    return conn;
}

static inline connection *delete_node(connection_vector *vector, int index) {
    if (vector->size * 4 < vector->capacity && vector->capacity > 2) {
        connection *new_buffer = (connection*) realloc(vector->buffer, sizeof(connection) * vector->capacity >> 1);
        if (new_buffer) {
            vector->capacity >>= 1;
            vector->buffer = new_buffer;

            for (size_t i = 0; i < vector->size; i++) {
                rectify_other_end(vector, i);
            }
        }
    }
    vector->size--;

    if (vector->buffer[index].type == NAND) {
        connection *other_end = vector->buffer[index].other_end;
        other_end->type = NONE;
        other_end->other_end = NULL;
    }

    if (!vector->size) {
        return NULL;
    }

    vector->buffer[index] = vector->buffer[vector->size];
    vector->buffer[index].index = index;
    rectify_other_end(vector, index);

    return &vector->buffer[index];
}

static inline void delete_connection_vector(connection_vector *vector) {
    if (!vector) {
        return;
    }

    while (vector->size) {
        delete_node(vector, 0);
    }

    free(vector->buffer);
    free(vector);
}

#endif