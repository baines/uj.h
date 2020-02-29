#include "../../uj_lex.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct state {
	struct uj_lexer p;
	int nesting;
	__uint128_t in_obj;
	bool obj_key;
};

bool uj_cb(struct uj_lexer* p, enum uj_type type, const char* data, size_t len){
	struct state* state = (struct state*)p;

	bool in_obj = state->in_obj & (1 << state->nesting);

#define indent (in_obj ? state->obj_key ? state->nesting * 2 : 0 : state->nesting * 2), ""

	switch(type){
		case UJ_NULL:
			printf("%*s\e[0;35mnull\e[0m,\n", indent);
			break;
		case UJ_BOOL_FALSE:
			printf("%*s\e[0;35mfalse\e[0m,\n", indent);
			break;
		case UJ_BOOL_TRUE:
			printf("%*s\e[0;35mtrue\e[0m,\n", indent);
			break;
		case UJ_INT:
			printf("%*s\e[0;33m%lld\e[0m,\n", indent, strtoll(strndupa(data, len), NULL, 10));
			break;
		case UJ_DOUBLE:
			printf("%*s\e[0;33m%g\e[0m,\n", indent, strtod(data, NULL));
			break;
		case UJ_STR:
			if(state->obj_key){
				printf("%*s\e[1;34m\"%.*s\"\e[0m: ", indent, (int)len, data);
			} else {
				printf("%*s\e[0;32m\"%.*s\"\e[0m,\n", indent, (int)len, data);
			}
			break;
		case UJ_OBJ: {
			printf("%*s{\n", indent);
			++state->nesting;
			assert(state->nesting < 128);
			state->in_obj |= (1 << state->nesting);
			state->obj_key = false;
			in_obj = true;
		} break;
		case UJ_OBJ_CLOSE: {
			state->in_obj &= ~(1 << state->nesting);
			--state->nesting;
			assert(state->nesting >= 0);
			printf("%*s},\n", indent);
			state->obj_key = false;
			in_obj = state->in_obj & (1 << state->nesting);
		} break;
		case UJ_ARR: {
			state->obj_key = false;
			printf("%*s[\n", indent);
			++state->nesting;
			assert(state->nesting < 128);
			in_obj = false;
		} break;
		case UJ_ARR_CLOSE: {
			--state->nesting;
			assert(state->nesting >= 0);
			printf("%*s],\n", indent);
			state->obj_key = false;
			in_obj = state->in_obj & (1 << state->nesting);
		} break;
	}

#undef indent

	if(in_obj){
		state->obj_key = !state->obj_key;
	}

	return true;
}

const char* status_msg(enum uj_status s){
	switch(s) {
		case UJ_OK: return "Ok";
		case UJ_PARTIAL: return "Partially tokenized";
		case UJ_ERR_OOM: return "Out of memory";
		case UJ_ERR_MALFORMED: return "Malformed input";
	}
	return "???";
}

int main(int argc, char** argv){
	char mem[BUFSIZ];
	char buf[BUFSIZ];

	struct state state = {};
	uj_lex_init(&state.p, mem, sizeof(mem));

	int status;
	for(;;){
		status = 0;

		if(!fgets(buf, sizeof(buf), stdin))
			break;

		status = uj_lex(&state.p, buf, strlen(buf), &uj_cb);

		if(status < UJ_OK){
			break;
		}
	}

	if(status == UJ_PARTIAL){
		status = uj_lex(&state.p, NULL, 0, &uj_cb);
	}

	fprintf(stderr, "Status: %s.\n", status_msg(status));
	return 0;
}
