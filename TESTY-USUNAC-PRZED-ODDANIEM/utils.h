#ifndef UTILS_H
#define UTILS_H
#include <stdlib.h>
#include <stdbool.h>
#include "nand.h"
// FIXME: dodać statici inliney zeby header only sie ladnie kompilowal/wywalic do utils.c
typedef enum {
    NONE,
    BOOL,
    NAND
} connection_type;

typedef union {
    nand_t *nand_ptr;
    const bool *bool_ptr;
} connection_value_ptr;

struct connection {
    connection_type type;
    connection_value_ptr value_ptr;
    size_t index;
    struct connection *other_end;
};

typedef struct connection connection;

typedef struct {
    connection *buffer;
    size_t size;
    size_t capacity;
} connection_vector;

static ssize_t max(ssize_t a, ssize_t b) {
    return a > b ? a : b;
}

static void rectify_other_end(connection_vector *vector, size_t i) {
    if(vector->buffer[i].type == NAND) {
        vector->buffer[i].other_end->other_end = &vector->buffer[i];
    }
}

static connection_vector *create_connection_vector() {
    connection_vector *vector = (connection_vector*) calloc(1, sizeof(connection_vector));
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

static connection* add_connection(connection_vector *vector) {
    if (vector->size >= vector->capacity) {
        connection *new_buffer = (connection*) realloc(vector->buffer, sizeof(connection) * vector->capacity << 1);
        if (!new_buffer) {
            return NULL;
        }
        vector->capacity <<= 1;
        vector->buffer = new_buffer;

        for(size_t i = 0; i < vector->size; i++) {
            rectify_other_end(vector, i);
        }
    }

    connection *conn = &vector->buffer[vector->size++];
    conn->type = NONE;
    conn->index = vector->size - 1;
    return conn;
}

static connection *delete_node(connection_vector *vector, int index) {
    vector->size--;
    /* FIXME: napisali w specyfikacji ze musi sie udac realloc na mniej?? chyba */
    if(vector->size * 4 < vector->capacity && vector->capacity > 2) {
        connection *new_buffer = (connection*) realloc(vector->buffer, sizeof(connection) * vector->capacity >> 1);
        if (new_buffer) {
            vector->capacity >>= 1;
            vector->buffer = new_buffer;

            for(size_t i = 0; i < vector->size; i++) {
                rectify_other_end(vector, i);
            }
        }
    }

    if(vector->buffer[index].type == NAND) {
        connection *other_end = vector->buffer[index].other_end;
        other_end->type = NONE;
    }

    if (!vector->size) {
        return NULL;
    }

    vector->buffer[index] = vector->buffer[vector->size];
    vector->buffer[index].index = index;
    if(vector->buffer[index].type == NAND) {
        vector->buffer[index].other_end->other_end = &vector->buffer[index];
    }

    return &vector->buffer[index];
}

static void delete_connection_vector(connection_vector *vector) {
    if(!vector) {
        return;
    }

    for (size_t i = 0; i < vector->size; i++) {
        delete_node(vector, i);
    }
    free(vector->buffer);
    free(vector);
}

#endif