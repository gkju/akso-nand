#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include "nand.h"
#include "utils.h"

typedef enum {
    INVALID,
    VALID,
    // Uzywane do wykrywania cykli w dfsie.
    IN_PROGRESS
} output_validity_type;

struct nand {
    bool output_value;
    output_validity_type output_validity;
    unsigned n;
    connection *inputs;
    connection_vector *outputs;
    ssize_t path_length;
};

typedef struct {
    bool value;
    bool error;
    ssize_t path_length;
} dfs_output;

nand_t* nand_new(unsigned n) {
    nand_t *gate = malloc(sizeof(nand_t));
    if (!gate) {
        errno = ENOMEM;
        return NULL;
    }
    
    gate->n = n;
    // Wywolanie calloc gwarantuje ustawienie inputow na NONE.
    gate->inputs = calloc(n, sizeof(connection));
    gate->outputs = create_connection_vector();
    if (!gate->outputs || !gate->inputs) {
        nand_delete(gate);
        errno = ENOMEM;
        return NULL;
    }
    for (unsigned i = 0; i < n; ++i) {
        gate->inputs[i].type = NONE;
        gate->inputs[i].index = i;
    }
    gate->output_value = false;
    gate->path_length = 0;
    gate->output_validity = INVALID;
    return gate;
}

void nand_delete(nand_t *g) {
    if (!g) {
        return;
    }
    
    if (g->inputs) {    
        for (unsigned i = 0; i < g->n; i++) {
            connection *c = &g->inputs[i];
            if (c->type == NAND) {
                nand_t *gate = c->value_ptr.nand_ptr;
                delete_node(gate->outputs, c->other_end->index);
            }
        }
        free(g->inputs);
    }

    delete_connection_vector(g->outputs);

    free(g);
}

static connection *disconnect_conn(connection *conn) {
    if (conn->type == NAND) {
        nand_t *gate = conn->value_ptr.nand_ptr;
        return delete_node(gate->outputs, conn->other_end->index);
    }
    return NULL;
}

int nand_connect_nand(nand_t *g_out, nand_t *g_in, unsigned k) {
    if (!g_out || !g_in || k >= g_in->n) {
        errno = EINVAL;
        return -1;
    }

    connection *conn = &g_in->inputs[k];
    // Jesli conn jest podpiety do g_out, to odlaczanie moze wywolac realloc na wektorze g_out->outputs.
    bool flag = false;
    if (conn->type == NAND) {
        flag = conn->value_ptr.nand_ptr == g_out;
    }
    connection *new_conn = add_connection(g_out->outputs);
    if (!new_conn) {
        errno = ENOMEM;
        return -1;
    }
    new_conn->type = NAND;
    new_conn->value_ptr.nand_ptr = g_in;
    new_conn->other_end = conn;
    // Jesli doszlo do wywolania realloc'a, to new_conn zostalo przesuniete w wektorze.
    connection *moved = disconnect_conn(conn);
    if (flag && moved) {
        new_conn = moved;
    }
    conn->type = NAND;
    conn->value_ptr.nand_ptr = g_out;
    conn->other_end = new_conn;
    return 0;
}

int nand_connect_signal(bool const *s, nand_t *g, unsigned k) {
    if (!s || !g || k >= g->n) {
        errno = EINVAL;
        return -1;
    }

    connection *conn = &g->inputs[k];
    disconnect_conn(conn);
    conn->type = BOOL;
    conn->value_ptr.bool_ptr = s;
    return 0;
}

static void set_validity(nand_t *g, bool unflag) {
    g->output_validity = unflag ? INVALID : VALID;
}

static bool read_validity(nand_t *g, bool unflag) {
    return unflag ? g->output_validity == INVALID : g->output_validity == VALID;
}

static dfs_output nand_dfs(nand_t *g, bool unflag) {
    dfs_output res;
    res.error = false;
    res.path_length = 0;
    res.value = false;

    if (g->output_validity == IN_PROGRESS) {
        errno = ECANCELED;
        res.error = true;
        return res;
    } else if (read_validity(g, unflag)) {
        res.value = g->output_value;
        res.path_length = g->path_length;
        return res;
    }

    g->output_validity = IN_PROGRESS;
    for (unsigned i = 0; i < g->n; ++i) {
        connection cur = g->inputs[i];
        bool inputval = false;
        if (cur.type == NONE) {
            res.error = true;
            errno = ECANCELED;
            set_validity(g, unflag);
            return res;
        } else if (cur.type == NAND) {
            nand_t *other_nand = cur.value_ptr.nand_ptr;
            dfs_output out = nand_dfs(other_nand, unflag);
            if (out.error) {
                res.value = out.value;
                res.error = true;
                set_validity(g, unflag);
                return res;
            }
            inputval = out.value;
            res.path_length = max(res.path_length, out.path_length);
        } else {
            inputval = *cur.value_ptr.bool_ptr;
        }

        if (!inputval) {
            res.value = true;
        }
    }
    
    if (g->n) {
        ++res.path_length;
    } 
    set_validity(g, unflag);
    g->output_value = res.value;
    g->path_length = res.path_length;
    return res;
}

ssize_t nand_evaluate(nand_t **g, bool *s, size_t m) {
    if (!m || !g || !s) {
        errno = EINVAL;
        return -1;
    }

    size_t dfs_cnt = 0, successful_dfs_cnt = 0;
    ssize_t crit_path_length = 0;
    for (size_t i = 0; i < m; ++i) {
        if (!g[i]) {
            errno = EINVAL;
            break;
        }

        dfs_output out = nand_dfs(g[i], false);
        ++dfs_cnt;
        if (out.error) {
            errno = ECANCELED;
            break;
        }

        s[i] = (bool) out.value;
        crit_path_length = max(crit_path_length, out.path_length);
        ++successful_dfs_cnt;
    }

    for (size_t i = 0; i < dfs_cnt; ++i) {
        nand_dfs(g[i], true);
    }

    if (successful_dfs_cnt < m) {
        return -1;
    }

    return crit_path_length;
}

ssize_t nand_fan_out(nand_t const *g) {
    if (!g) {
        errno = EINVAL;
        return -1;
    }

    return g->outputs->size;
}

void* nand_input(nand_t const *g, unsigned k) {
    if (!g || k >= g->n) {
        errno = EINVAL;
        return NULL;
    }

    connection* conn = &g->inputs[k];
    if (conn->type == BOOL) {
        return (void*) conn->value_ptr.bool_ptr;
    } else if (conn->type == NAND) {
        return (void*) conn->value_ptr.nand_ptr;
    }

    errno = 0;
    return NULL;
}

nand_t* nand_output(nand_t const *g, ssize_t k) {
    if (!g || k < 0 || k >= (ssize_t) g->outputs->size) {
        errno = EINVAL;
        return NULL;
    }

    return g->outputs->buffer[k].value_ptr.nand_ptr;
}