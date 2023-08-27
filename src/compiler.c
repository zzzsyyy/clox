#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "value.h"

#define THREE_BYTE_MAX 16777216 //2^24

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
	Token previous;
	Token current;
	bool had_err;
	bool panic_mode;
} Parser;

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR,         // or
	PREC_AND,        // and
	PREC_EQUALITY,   // == !=
	PREC_COMPARISON,  // < > <= >=
	PREC_TERM,       // + -
	PREC_FACTOR,     // * /
	PREC_UNARY,      // - !
	PREC_CALL,       // . ()
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compiling_chunk;

static Chunk *current_chunk() {
	return compiling_chunk;
}

static void error_at(Token *token, const char *msg) {
	if (parser.panic_mode) return;
	parser.panic_mode = true;
	fprintf(stderr, "[line %d] Error", token->line);
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR){
		// hien
	}else {
		fprintf(stderr, " at '%.*s", token->length, token->start);
	}
	fprintf(stderr, ": %s\n", msg);
	parser.had_err = true;
}

static void error_at_current(const char *msg) {
	error_at(&parser.current, msg);
}

static void error(const char* msg) {
	error_at(&parser.previous, msg);
}

static void advance() {
	parser.previous = parser.current;
	for (;;) {
		parser.current = scan_token();
		if (parser.current.type != TOKEN_ERROR) break;
		error_at_current(parser.current.start);
	}
}

static void consume(TokenType type, const char *msg) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	error_at_current(msg);
}

static void emit_byte(uint8_t byte) {
	write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
	emit_byte(byte1);
	emit_byte(byte2);
}

static void emit_return() {
	emit_byte(OP_RETURN);
}

static uint8_t make_constant(Value value) {
	int constant = add_constant(current_chunk(), value);
	return (uint8_t)constant;
}

static void writeConstant(Chunk *chunk, Value value, int line) {
	uint32_t idx = (uint32_t)add_constant(chunk, value);
	if (idx > THREE_BYTE_MAX) {
		error("Too many constants in one chunk. Maximum allowed are 2^24.");
	}
	// use >> &0xff devide 8 8 8
	emit_bytes(OP_CONSTANT_LONG, (uint8_t)(idx & 0xff));
	emit_bytes((uint8_t)((idx >> 8) & 0xff), (uint8_t)((idx >> 16) & 0xff));
}

static void emit_constant(Value value) {
	if (current_chunk()->constants.count > UINT8_MAX) {
		writeConstant(current_chunk(), value, parser.previous.line);
	} else {
		emit_bytes(OP_CONSTANT, make_constant(value));
	}
}

static void end_compiler() {
	emit_return();
#ifdef DEBUG_PRINT_CODE
	if (!parser.had_err){
		disassemble_chunk(current_chunk(), "code");
	}
#endif
}

static void expression();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

static void binary() {
	TokenType operator_type = parser.previous.type;
	ParseRule *rule = get_rule(operator_type);
	parse_precedence((Precedence)(rule->precedence + 1));
	switch (operator_type) {
	case TOKEN_BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT); break;
	case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
	case TOKEN_GREATER:       emit_byte(OP_GREATER); break;
	case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
	case TOKEN_LESS:          emit_byte(OP_LESS); break;
	case TOKEN_LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
	case TOKEN_PLUS:          emit_byte(OP_ADD); break;
	case TOKEN_MINUS:         emit_byte(OP_SUBTRACT); break;
	case TOKEN_STAR:          emit_byte(OP_MULTIPLY); break;
	case TOKEN_SLASH:         emit_byte(OP_DIVIDE); break;
	default: return;
	}
}

static void literal() {
	switch (parser.previous.type) {
		case TOKEN_FALSE: emit_byte(OP_FALSE); break;
		case TOKEN_NIL: emit_byte(OP_NIL); break;
		case TOKEN_TRUE: emit_byte(OP_TRUE); break;
		default: return;
	}
}

static void grouping() {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
	double value = strtod(parser.previous.start, NULL);
	emit_constant(NUMBER_VAL(value));
}

static void unary() {
	TokenType operator_type = parser.previous.type;
	parse_precedence(PREC_UNARY);
	switch (operator_type) {
		case TOKEN_BANG: emit_byte(OP_NOT); break;
		case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
		default: return;
	}
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
	[TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
	[TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
	[TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
	[TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
	[TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
	[TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
	[TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
	[TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
	[TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
	[TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
	[TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
	[TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
	[TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
	[TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
	[TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_TRUE]          = {literal,     NULL,   PREC_NONE},
	[TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parse_precedence(Precedence precedence) {
	advance();
	ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
	if (prefix_rule == NULL) {
		error("Expect expression.");
		return;
	}
	prefix_rule();
	while (precedence <= get_rule(parser.current.type)->precedence){
		advance();
		ParseFn infix_rule = get_rule(parser.previous.type)->infix;
		infix_rule();
	}
}

static ParseRule *get_rule(TokenType type) {
	return &rules[type];
}

static void expression() {
	parse_precedence(PREC_ASSIGNMENT);
}

bool compile(const char *src, Chunk *chunk) {
	init_scanner(src);
	compiling_chunk = chunk;
	parser.had_err = false;
	parser.panic_mode = false;
	advance();
	expression();
	consume(TOKEN_EOF, "Expect end of expression.");
	end_compiler();
	return !parser.had_err;
	// int line = -1;
	// for (;;) {
	// 	Token token = scan_token();
	// 	if (token.line != line) {
	// 		printf("%4d ", token.line);
	// 		line = token.line;
	// 	} else {
	// 		printf("   | ");
	// 	}
	// 	printf("%2d '%.*s'\n", token.type, token.length, token.start);

	// 	if (token.type == TOKEN_EOF) break;
	// }
}
