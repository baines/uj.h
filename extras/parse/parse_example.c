#include "../../uj_lex.h"
#include "../../uj_parse.h"
#include <stdio.h>
#include <stdbool.h>

char* json_escape(const char* str) {
	size_t len = strlen(str);
	char* result = malloc(len*6+1);
	char* q = result;

	static const char from[] = "\"\\\b\f\n\r\t";
	static const char to[]   = "\"\\bfnrt";
	static const char hex[]  = "0123456789abcdef";

	for(const uint8_t* s = str; *s; ++s) {
		char* m = memchr(from, *s, sizeof(from));
		if(m) {
			*q++ = '\\';
			*q++ = to[m - from];
		} else if(*s < 0x20) {
			*q++ = '\\';
			*q++ = 'u';
			*q++ = '0';
			*q++ = '0';
			*q++ = hex[*s >> 4];
			*q++ = hex[*s & 15];
		} else {
			*q++ = *s;
		}
	}

	*q++ = '\0';

	return result;
}

static void json_print(struct uj_node* n, int indent, bool key) {

#define pr(fmt, ...) printf("%*s" fmt, key ? 0 : indent, "", ##__VA_ARGS__), key=false

	switch(n->type) {
		case UJ_NULL: {
			pr("null");
		} break;
		case UJ_BOOL_FALSE: {
			pr("false");
		} break;
		case UJ_BOOL_TRUE: {
			pr("true");
		} break;
		case UJ_INT: {
			pr("%" PRIiMAX, n->num);
		} break;
		case UJ_STR: {
			char* escaped = json_escape(n->str);
			pr("\"%s\"", escaped);
			free(escaped);
		} break;
		case UJ_DOUBLE: {
			pr("%.2f", n->dbl);
		} break;
		case UJ_OBJ: {
			pr("{\n");
			indent += 2;
			for(struct uj_kv* kv = n->obj; kv; kv = kv->next) {
				if(kv != n->obj) {
					printf(",\n");
				}
				char* escaped = json_escape(kv->key);
				pr("\"%s\": ", escaped);
				free(escaped);
				json_print(kv->val, indent, true);
			}
			indent -= 2;
			printf("\n");
			pr("}");
		} break;
		case UJ_ARR: {
			pr("[\n");
			for(size_t i = 0; i < n->len; ++i) {
				if(i > 0) {
					printf(",\n");
				}
				json_print(n->arr + i, indent + 2, false);
			}
			printf("\n");
			pr("]");
		} break;
	}

#undef pr
}

#define CHUNKED 1

int main(void) {
	char buf[BUFSIZ];
	enum uj_status err;

#if CHUNKED
	struct uj_parser p = {};

	while(fgets(buf, sizeof(buf), stdin)) {
		if((err = uj_parse_chunk(&p, buf, strlen(buf))) < UJ_OK)
			break;
	}

	struct uj_node* root = uj_parse_chunk_end(&p);
#else
	char* data = NULL;
	size_t len = 0;

	while(fgets(buf, sizeof(buf), stdin)) {
		size_t n = strlen(buf);
		data = realloc(data, len + n);
		memcpy(data + len, buf, n);
		len += n;
	}

	struct uj_node* root = uj_parse(data, len, &err);
	free(data);
#endif

	if(root) {
		json_print(root, 0, false);
		puts("");
		uj_node_free(root, true);
	} else {
		fprintf(stderr, "ERROR %d\n", err);
	}
}
