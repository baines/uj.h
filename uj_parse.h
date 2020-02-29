#include <stdlib.h>
#include <inttypes.h>

#ifndef UJ_PARSE_BUF_SIZE
#define UJ_PARSE_BUF_SIZE 4096
#endif

#ifndef UJ_PARSE_MAX_DEPTH
#define UJ_PARSE_MAX_DEPTH 256
#endif

struct uj_kv {
	char* key;
	struct uj_node* val;
	struct uj_kv* next;
};

struct uj_node {
	enum uj_type            type;
	union {
		intmax_t            num;
		char*               str;
		double              dbl;
		struct uj_kv*       obj;
		struct {
			struct uj_node* arr;
			size_t          len;
		};
	};
};

struct uj_level {
	enum uj_type       type;
	struct uj_node** next;
	union {
		struct uj_kv** next_kv;
		size_t* len;
	};
};

struct uj_parser {
	struct uj_lexer uj;
	struct uj_level* lvl;
	struct uj_level level_storage[UJ_PARSE_MAX_DEPTH];
	struct uj_node* root_node;
	enum uj_status err;
	_Bool initialized;
	char mem[UJ_PARSE_BUF_SIZE];
};

///////////////////////

struct uj_node*  uj_parse           (const char* json , size_t len, enum uj_status* err);
enum   uj_status uj_parse_chunk     (struct uj_parser*, const char* chunk, size_t len);
struct uj_node*  uj_parse_chunk_end (struct uj_parser*);
void             uj_node_free       (struct uj_node*, _Bool free_self);

///////////////////////

static inline _Bool uj_parser_cb(struct uj_lexer* p, enum uj_type type, const char* data, size_t len){
	struct uj_parser* j = (struct uj_parser*)p;
	struct uj_level* l = j->lvl;

#define fail(x) return j->err = (x), 0

	if(l->type == UJ_OBJ && !l->next) {
		if(type == UJ_STR) {
			*l->next_kv = calloc(1, sizeof(**l->next_kv));
			(*l->next_kv)->key = strndup(data, len);
			l->next = &(*l->next_kv)->val;
			l->next_kv = &(*l->next_kv)->next;
		} else if(type == UJ_OBJ_CLOSE) {
			j->lvl--;
		} else {
			fail(UJ_ERR_EXPECTED_KEY);
		}
	} else if(l->type == UJ_ARR && type == UJ_ARR_CLOSE) {
		j->lvl--;
	} else {
		if(!l->next)
			return 0;

		struct uj_node* node;
		if(l->type == UJ_ARR) {
			*l->next = realloc(*l->next, (*l->len + 1) * sizeof(**l->next));
			node = &(*l->next)[(*l->len)++];
			memset(node, 0, sizeof(*node));
		} else {
			node = calloc(1, sizeof(**l->next));
		}

		node->type = type;

		switch(type) {
			case UJ_STR: {
				node->str = strndup(data, len);
			} break;
			case UJ_INT: {
				node->num = strtoimax(strndupa(data, len), NULL, 10);
			} break;
			case UJ_DOUBLE: {
				node->dbl = strtod(strndupa(data, len), NULL);
			} break;
			case UJ_ARR: {
				if(++j->lvl - j->level_storage == UJ_PARSE_MAX_DEPTH)
					fail(UJ_ERR_TOO_DEEP);
				j->lvl->type = type;
				j->lvl->next = &node->arr;
				j->lvl->len = &node->len;
			} break;
			case UJ_OBJ: {
				if(++j->lvl - j->level_storage == UJ_PARSE_MAX_DEPTH)
					fail(UJ_ERR_TOO_DEEP);
				j->lvl->type = type;
				j->lvl->next = NULL;
				j->lvl->next_kv = &node->obj;
			} break;
			case UJ_ARR_CLOSE: fail(UJ_ERR_UNEXPECTED_ARR_CLOSE);
			case UJ_OBJ_CLOSE: fail(UJ_ERR_UNEXPECTED_OBJ_CLOSE);
		}

		if(l->type != UJ_ARR) {
			*l->next = node;
			l->next = NULL;
		}
	}

#undef fail

	return 1;
}

enum uj_status uj_parse_chunk(struct uj_parser* p, const char* chunk, size_t len) {
	if(!p->initialized) {
		p->initialized = 1;
		uj_lex_init(&p->uj, p->mem, sizeof(p->mem));
		p->lvl = p->level_storage;
		p->lvl->next = &p->root_node;
	}

	if(p->err < UJ_OK) {
		return p->err;
	}

	enum uj_status status = uj_lex(&p->uj, chunk, len, &uj_parser_cb);

	if(p->err == UJ_OK) {
		p->err = status;
	}

	return p->err;
}

struct uj_node* uj_parse_chunk_end(struct uj_parser* p) {
	if(p->err == UJ_PARTIAL) {
		p->err = uj_lex(&p->uj, NULL, 0, &uj_parser_cb);
	}

	if(p->err != UJ_OK) {
		uj_node_free(p->root_node, 1);
		return NULL;
	}

	return p->root_node;
}

struct uj_node* uj_parse(const char* json, size_t len, enum uj_status* err) {
	struct uj_parser parser = {};
	enum uj_status status = uj_parse_chunk(&parser, json, len);
	if(status != UJ_OK && err) {
		*err = status;
	}
	return uj_parse_chunk_end(&parser);
}

void uj_node_free(struct uj_node* n, _Bool free_self) {
	if(!n)
		return;

	switch(n->type) {
		case UJ_STR: {
			free(n->str);
		} break;
		case UJ_OBJ: {
			struct uj_kv* next;
			for(struct uj_kv* kv = n->obj; kv; kv = next) {
				next = kv->next;
				free(kv->key);
				uj_node_free(kv->val, 1);
				free(kv);
			}
		} break;
		case UJ_ARR: {
			for(size_t i = 0; i < n->len; ++i) {
				uj_node_free(n->arr + i, 0);
			}
			free(n->arr);
		} break;
	}

	if(free_self)
		free(n);
}
