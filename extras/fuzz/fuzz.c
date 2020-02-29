#define UJ_PARSE_BUF_SIZE 31
#define UJ_PARSE_MAX_DEPTH 4
#include "../../uj.h"
#include <stdio.h>

int main(void) {
	char buf[4096];
	char* data = NULL;
	size_t len = 0;
	struct uj_parser p = {};

	while(fgets(buf, sizeof(buf), stdin)) {
		size_t n = strlen(buf);
		data = realloc(data, len + n);
		memcpy(data + len, buf, n);
		len += n;
		uj_parse_chunk(&p, buf, n);
	}

	enum uj_status err;
	struct uj_node* root = uj_parse(data, len, (len & 1) ? &err : NULL);
	uj_node_free(root, 1);
	free(data);
}
