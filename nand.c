#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
// FIXME: usunac zbedne includy
#include <stdio.h>
#include "nand.h"
#include "utils.h"

typedef enum {
    INVALID,
    VALID,
    // do wykrywania cykli w dfsie
    IN_PROGRESS
} output_validity_type;

struct nand {
    unsigned n;
    connection *inputs;
    connection_vector *outputs;
    bool output_value;
    ssize_t path_length;
    output_validity_type output_validity;
};

typedef struct {
    ssize_t path_length;
    int8_t value;
} dfs_output;

nand_t* nand_new(unsigned n) {
    nand_t *gate = malloc(sizeof(nand_t));
    if (!gate) {
        errno = ENOMEM;
        return NULL;
    }
    
    gate->n = n;
    // calloc gwarantuje ustawienie inputow na NONE
    gate->inputs = calloc(n, sizeof(connection));
    for(unsigned i = 0; i < n; ++i) {
        gate->inputs[i].type = NONE;
        gate->inputs[i].index = i;
    }
    gate->outputs = create_connection_vector();
    if (!gate->outputs || !gate->inputs) {
        nand_delete(gate);
        errno = ENOMEM;
        return NULL;
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
        for(unsigned i = 0; i < g->n; i++) {
            connection *c = &g->inputs[i];
            if (c->type == NAND) {
                nand_t *gate = c->value_ptr.nand_ptr;
                // FIXME: usunac debug
                printf("deleting connection from %p to %p\n", gate, g); 
                delete_node(gate->outputs, c->other_end->index);
            }
        }
        free(g->inputs);
    }
    if (g->outputs) {
        delete_connection_vector(g->outputs);
    }

    free(g);
}

connection *disconnect_conn(connection *conn) {
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
    // Jesli conn jest podpiety do g_out, to odlaczanie moze wywolac realloc na wektorze g_out->outputs
    bool flag = conn->value_ptr.nand_ptr == g_out;
    connection *new_conn = add_connection(g_out->outputs);
    if (!new_conn) {
        errno = ENOMEM;
        return -1;
    }
    new_conn->type = NAND;
    new_conn->value_ptr.nand_ptr = g_in;
    new_conn->other_end = conn;
    // Jesli doszlo do wywolania realloc'a, to new_conn zostalo przesuniete w wektorze
    connection *moved = disconnect_conn(conn);
    if(flag && moved) {
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

void set_validity(nand_t *g, bool unflag) {
    g->output_validity = unflag ? INVALID : VALID;
}

dfs_output nand_dfs(nand_t *g, bool unflag) {
    dfs_output res;

    // FIXME: obecnie troche niespojnie raz uzywam enuma jak enum a raz jak inta, ogarnac jak najlepiej to robic
    if(g->output_validity == IN_PROGRESS) {
        errno = ECANCELED;
        res.value = -1;
        return res;
    } else if(g->output_validity ^ unflag) {
        res.value = g->output_value;
        res.path_length = g->path_length;
        return res;
    }

    res.path_length = 0;
    res.value = false;
    g->output_validity = IN_PROGRESS;
    for(unsigned i = 0; i < g->n; ++i) {
        connection cur = g->inputs[i];
        bool inputval = false;
        if(cur.type == NONE) {
            res.value = -1;
            errno = EINVAL;
            set_validity(g, unflag);
            return res;
        } else if(cur.type == NAND) {
            nand_t *other_nand = cur.value_ptr.nand_ptr;
            dfs_output out = nand_dfs(other_nand, unflag);
            inputval = out.value;
            res.path_length = max(res.path_length, out.path_length);
        } else {
            inputval = *cur.value_ptr.bool_ptr;
        }

        if(!inputval) {
            res.value = true;
        }
    }

    ++res.path_length;
    set_validity(g, unflag);
    return res;
}

ssize_t nand_evaluate(nand_t **g, bool *s, size_t m) {
    if(!m) {
        errno = EINVAL;
        return -1;
    }

    size_t successfull_dfs_cnt = 0;
    ssize_t crit_path_length = 0;
    for(size_t i = 0; i < m; ++i) {
        if(!g[i]) {
            errno = EINVAL;
            break;
        }

        dfs_output out = nand_dfs(g[i], false);
        if(out.value == -1) {
            errno = ECANCELED;
            break;
        }

        s[i] = (bool) out.value;
        crit_path_length = max(crit_path_length, out.path_length);
        ++successfull_dfs_cnt;
    }

    for(size_t i = 0; i < successfull_dfs_cnt; ++i) {
        nand_dfs(g[i], true);
    }

    if(successfull_dfs_cnt < m) {
        return -1;
    }

    return crit_path_length;
}

ssize_t nand_fan_out(nand_t const *g) {
    if(!g) {
        errno = EINVAL;
        return -1;
    }

    return g->outputs->size;
}

void* nand_input(nand_t const *g, unsigned k) {
    if(!g || k >= g->n) {
        errno = EINVAL;
        return NULL;
    }

    connection* conn = &g->inputs[k];
    if(conn->type == BOOL) {
        return (void*) conn->value_ptr.bool_ptr;
    } else if(conn->type == NAND) {
        return (void*) conn->value_ptr.nand_ptr;
    }

    errno = 0;
    return NULL;
}

nand_t* nand_output(nand_t const *g, ssize_t k) {
    if(!g || k < 0 || k >= (ssize_t) g->outputs->size) {
        errno = EINVAL;
        return NULL;
    }

    return g->outputs->buffer[k].value_ptr.nand_ptr;
}