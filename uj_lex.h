#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

enum uj_status {
	/* lexer & parser */
	UJ_OK                       = 0,
	UJ_PARTIAL                  = 1,

	UJ_ERR_OOM                  = -2,
	UJ_ERR_MALFORMED            = -3,
	/* parser only */
	UJ_ERR_EXPECTED_KEY         = -4,
	UJ_ERR_UNEXPECTED_OBJ_CLOSE = -5,
	UJ_ERR_UNEXPECTED_ARR_CLOSE = -6,
	UJ_ERR_TOO_DEEP             = -41,
};

struct uj_lexer {
	uint_fast32_t state;
	uint_fast32_t num_state;
	uint_fast32_t udigits;
	uint_fast32_t ucode;

	const char* literal;

	char* work_mem;
	char* work_cur;
	char* work_end;
};

typedef _Bool (*uj_lex_callback)(struct uj_lexer*, enum uj_type, const char* data, size_t len);

///////////////////////

static void    uj_lex_init (struct uj_lexer*, char* work_mem, size_t work_mem_size);
enum uj_status uj_lex      (struct uj_lexer*, const char* json, size_t len, uj_lex_callback cb);

///////////////////////

static void uj_lex_init(struct uj_lexer* uj, char* work_mem, size_t work_mem_size){
	memset(uj, 0, sizeof(*uj));
	uj->work_mem = work_mem;
	uj->work_cur = work_mem;
	uj->work_end = work_mem + work_mem_size;
}

static inline int uj_lex_num(struct uj_lexer* uj, const char** json, size_t uj_len, uj_lex_callback callback, _Bool eof){
	const char* end = *json + uj_len;

	if(eof)
		goto eof_check;

	enum {
		UJ_NS_INITIAL,
		UJ_NS_ZERO,
		UJ_NS_DIGIT,
		UJ_NS_MANTISSA,
		UJ_NS_EXP_SIGN,
		UJ_NS_EXPONENT,
	};

	while(*json != end){
		char c = **json;
		int new_state = -1;

		if(c == ',' || c == '}' || c == ']' || c == ':' || c == ' ' || c == '\t' || c == '\r' || c == '\n'){
			int rv;
eof_check:
			switch(uj->num_state){
				case UJ_NS_ZERO:
				case UJ_NS_DIGIT: {
					rv = callback(uj, UJ_INT, uj->work_mem, uj->work_cur - uj->work_mem);
				} break;
				case UJ_NS_MANTISSA:
				case UJ_NS_EXPONENT: {
					rv = callback(uj, UJ_DOUBLE, uj->work_mem, uj->work_cur - uj->work_mem);
				} break;
				default:
					rv = UJ_ERR_MALFORMED;
			}

			--*json;
			uj->work_cur = uj->work_mem;
			uj->num_state = UJ_NS_INITIAL;
			return rv;
		}

		switch(uj->num_state){
			default:/* case UJ_NS_INITIAL:*/ {
				if(c == '0')
					new_state = UJ_NS_ZERO;
				else if(c >= '1' && c <= '9')
					new_state = UJ_NS_DIGIT;
			} break;
			case UJ_NS_ZERO: {
				if(c == '.')
					new_state = UJ_NS_MANTISSA;
				else if(c == 'e' || c == 'E')
					new_state = UJ_NS_EXP_SIGN;
			} break;
			case UJ_NS_DIGIT: {
				if(c >= '0' && c <= '9')
					new_state = UJ_NS_DIGIT;
				else if(c == '.')
					new_state = UJ_NS_MANTISSA;
				else if(c == 'e' || c == 'E')
					new_state = UJ_NS_EXP_SIGN;
			} break;
			case UJ_NS_MANTISSA: {
				if(c >= '0' && c <= '9')
					new_state = UJ_NS_MANTISSA;
				else if(c == 'e' || c == 'E')
					new_state = UJ_NS_EXP_SIGN;
			} break;
			case UJ_NS_EXP_SIGN: {
				if(c == '+' || c == '-' || (c >= '0' && c <= '9'))
					new_state = UJ_NS_EXPONENT;
			} break;
			case UJ_NS_EXPONENT: {
				if(c >= '0' && c <= '9')
					new_state = UJ_NS_EXPONENT;
			} break;
		}

		if(new_state == -1){
			uj->num_state = UJ_NS_INITIAL;
			return UJ_ERR_MALFORMED;
		}

		if(uj->work_cur == uj->work_end){
			uj->num_state = UJ_NS_INITIAL;
			return UJ_ERR_OOM;
		}

		uj->num_state = new_state;
		*uj->work_cur++ = *(*json)++;
	}

	return UJ_PARTIAL;
}

static inline char uj_lower(char c){
	if(c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');
	return c;
}

int uj_lex(struct uj_lexer* uj, const char* json, size_t len, uj_lex_callback callback){
	const char* p   = json;
	const char* end = json + len;
	const _Bool eof = len == 0;

	goto *&&resume_start + uj->state;

	for(;; ++p){
		uj->state = 0;

resume_start:
		if(p == end)
			break;

		if(*p == '\r' || *p == '\n' || *p == '\t' || *p == ' ' || *p == ':' || *p == ',')
			continue;

		static const char objarr[] = "{}[]";
		const char* c;
		int rv;

		uj->literal =
			*p == 't' ? "true" :
			*p == 'f' ? "false" :
			*p == 'n' ? "null" :
			NULL;

		if(uj->literal){
			uj->state = &&resume_literal - &&resume_start;
resume_literal:
			for(;;){
				if(eof || *p != *uj->literal)
					return UJ_ERR_MALFORMED;

				if(!*++uj->literal){
					break;
				}

				if(++p == end)
					return UJ_PARTIAL;
			}

			char k = uj->literal[-2];
			int type = UJ_NULL + (k == 's') + ((k == 'u') << 1);
			callback(uj, type, NULL, 0);
			uj->literal = NULL;
		}

		else if(*p == '-' || (*p >= '0' && *p <= '9')){
			if(*p == '-'){
				*uj->work_cur++ = '-';
				++p;
			}
			uj->state = &&resume_number - &&resume_start;
resume_number:
			rv = uj_lex_num(uj, &p, end - p, callback, eof);
			if(rv <= 0)
				return rv;
		}

		else if(*p == '"'){
			uj->state = &&resume_str - &&resume_start;
			++p;
resume_str:
			for(;;){
				if(p == end)
					return UJ_PARTIAL;

				if(*p == '"'){
					rv = callback(uj, UJ_STR, uj->work_mem, uj->work_cur - uj->work_mem);
					uj->work_cur = uj->work_mem;
					break;
				} else if(*p == '\\'){
					static const char from[] = "\"\\/bfnrt";
					static const char to[]   = "\"\\/\b\f\n\r\t";
					uj->state = &&resume_str_unescape_1 - &&resume_start;
					++p;
resume_str_unescape_1:
					if(p == end)
						return UJ_PARTIAL;

					if(uj->work_cur == uj->work_end)
						return UJ_ERR_OOM;

					if((c = memchr(from, *p, sizeof(from)))){
						++p;
						*uj->work_cur++ = to[c - from];
						uj->state = &&resume_str - &&resume_start;
					} else if(*p == 'u') {
						++p;
						uj->state = &&resume_str_unescape_2 - &&resume_start;
resume_str_unescape_2:
						for(; uj->udigits < 4; ++uj->udigits){
							if(p == end)
								return UJ_PARTIAL;

							static const char hex[] = "0123456789abcdef";
							c = memchr(hex, uj_lower(*p), sizeof(hex));
							if(!c)
								return UJ_ERR_MALFORMED;

							uj->ucode = (uj->ucode << 4) | (c - hex);
							++p;
						}

						// convert to utf-8
						static const size_t counts[] = { 0x7f, 0x7ff };
						int i;
						for(i = 0; i < 2; ++i){
							if(uj->ucode <= counts[i])
								break;
						}

						if(uj->work_end - uj->work_cur < (i+1))
							return UJ_ERR_OOM;

						if(i == 0){
							*uj->work_cur++ = uj->ucode;
						} else if(i == 1){
							*uj->work_cur++ = (uj->ucode >> 6) | 0xc0;
							*uj->work_cur++ = (uj->ucode & 0x3f) | 0x80;
						} else {
							*uj->work_cur++ = (uj->ucode >> 12) | 0xe0;
							*uj->work_cur++ = ((uj->ucode >> 6) & 0x3f) | 0x80;
							*uj->work_cur++ = (uj->ucode & 0x3f) | 0x80;
						}

						uj->udigits = uj->ucode = 0;
						uj->state = &&resume_str - &&resume_start;
					} else {
						return UJ_ERR_MALFORMED;
					}
				} else {
					if(uj->work_cur == uj->work_end)
						return UJ_ERR_OOM;

					*uj->work_cur++ = *p++;
				}
			}
		} else if((c = memchr(objarr, *p, sizeof(objarr)))){
			rv = callback(uj, UJ_OBJ + (c - objarr), NULL, 0);
			if(rv <= 0)
				return rv;
		} else {
			return UJ_ERR_MALFORMED;
		}
	}

	return UJ_OK;
}
