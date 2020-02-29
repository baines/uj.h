## uj.h

### Single-header C99 JSON lexer / parser in ~500 LoC

- `uj_lex.h` - lexer, coverts json string to tokens. (can be used standalone)
- `uj_parse.h` - parser, creates tree structure from json string.
- `uj.h` - single header, concatenation of above.

#### Features

- Not strict, accepts trailing commas
- Does not have [this problem](https://nullprogram.com/blog/2019/12/28/)
  - 01 is treated as a single invalid token by the lexer not two tokens
- Can lex/parse partial chunks of json, e.g. as they download
- Lexer can be used without dynamic memory allocation, see extras/lex
- Fuzzed with AFL, 100% branch coverage and no crashes, see extras/fuzz
  - But still probably has some bugs...

#### API

##### Functions

```c
// lexer
// sends tokens to given callback.
// call uj_lex once or more with chunks of JSON (boundaries don't matter)

typedef _Bool (*uj_lex_callback)(struct uj_lexer*, enum uj_type, const char* data, size_t len);

static void    uj_lex_init (struct uj_lexer*, char* work_mem, size_t work_mem_size);
enum uj_status uj_lex      (struct uj_lexer*, const char* json, size_t len, uj_lex_callback cb);

// parser
// generates uj_node* tree structure from input
// uj_parse is a one-shot parse of a full JSON string.
// uj_parse_chunk accepts arbitrary partial chunks, call uj_parse_chunk_end when you're done.

struct uj_node*  uj_parse           (const char* json , size_t len, enum uj_status* err);
enum   uj_status uj_parse_chunk     (struct uj_parser*, const char* chunk, size_t len);
struct uj_node*  uj_parse_chunk_end (struct uj_parser*);
void             uj_node_free       (struct uj_node*, _Bool free_self);
```

##### Main Enums / Structs

```c
enum uj_type {
	UJ_NULL,
	UJ_BOOL_FALSE,
	UJ_BOOL_TRUE,
	UJ_INT,
	UJ_DOUBLE,
	UJ_STR,
	UJ_OBJ,
	UJ_OBJ_CLOSE,
	UJ_ARR,
	UJ_ARR_CLOSE,
};

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
```

