#include "../../include/lexer/token.h"
#include "../../include/lexer/token_types.h"
#include "../../include/lexer/lexer.h"
#include "../../include/parser/safe_alloc.h" // includes the safe_malloc function
#include <cctype>
#include <iostream>
#include <fstream>
#include <cstring>

#define MAX_LEXEME_LENGTH 256
#define KEYWORD_COUNT 25
#define MAX_UNGET_TOKENS 256 // I actually don't know why I chose this number. Forgive me...

int evaluate_expression(Token *condition);
int unget_buffer_index = 0;

Token* unget_buffer[MAX_UNGET_TOKENS];

void lex_unget_token(Lexer* lexer, Token* token) {
    if (unget_buffer_index < MAX_UNGET_TOKENS) {
        unget_buffer[unget_buffer_index++] = token;
    } else {
        std::cerr << "Error: unget buffer overflow" << std::endl;
    }
}

// Hash table for keywords
struct Keyword {
    const char* word;
    TokenType type;
};

const Keyword keywords[KEYWORD_COUNT] = {
    {"int", TOKEN_INT}, {"str", TOKEN_STR}, {"bool", TOKEN_BOOL},
    {"let", TOKEN_LET}, {"const", TOKEN_CONST}, {"function", TOKEN_FUNCTION},
    {"if", TOKEN_IF}, {"else", TOKEN_ELSE}, {"for", TOKEN_FOR},
    {"while", TOKEN_WHILE}, {"return", TOKEN_RETURN}, 
    {"class", TOKEN_CLASS}, {"import", TOKEN_IMPORT},
    {"true", TOKEN_TRUE}, {"false", TOKEN_FALSE}, {"null", TOKEN_NULL},
    {"print", TOKEN_PRINT}
};

// Create a new lexer
Lexer* create_lexer(std::ifstream& file) {
    Lexer* lexer = (Lexer*)safe_malloc(sizeof(Lexer));
    lexer->file = &file;
    lexer->current_char = file.get();
    lexer->line = 1;
    lexer->column = 1;
    return lexer;
}

// Destroy the lexer
void destroy_lexer(Lexer* lexer) {
    if (lexer) {
        delete lexer->file;
        delete lexer;
    }
}

// Advance to the next character
int lex_advance(Lexer* lexer) {
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    lexer->current_char = lexer->file->get();
    return lexer->current_char;
}

// Skip whitespace
void skip_whitespace(Lexer* lexer) {
    while (std::isspace(lexer->current_char)) {
        lex_advance(lexer);
    }
}

// Skip comments
void skip_comments(Lexer* lexer) {
    if (lexer->current_char == '/') {
        lex_advance(lexer); // Advance past first '/'
        if (lexer->current_char == '/') {
            // Single-line comment
            while (lexer->current_char != '\n' && lexer->current_char != EOF) {
                lex_advance(lexer);
            }
            if (lexer->current_char == '\n') {
                lex_advance(lexer); // Skip the newline
            }
        } else if (lexer->current_char == '*') {
            // Multi-line comment
            lex_advance(lexer); // Advance past '*'
            while (lexer->current_char != EOF) {
                if (lexer->current_char == '*') {
                    lex_advance(lexer);
                    if (lexer->current_char == '/') {
                        lex_advance(lexer);
                        break;
                    }
                } else {
                    lex_advance(lexer);
                }
            }
        }
    }
}

// Keyword lookup
TokenType lookup_keyword(const std::string& lexeme) {
    for (int i = 0; i < KEYWORD_COUNT; i++) {
        if (lexeme == keywords[i].word) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

Token* lex_identifier_or_keyword(Lexer* lexer) {
    char lexeme[MAX_LEXEME_LENGTH] = {0};
    int length = 0;
    int start_column = lexer->column;

    // Store the first character
    lexeme[length++] = lexer->current_char;
    lex_advance(lexer);

    // Read the rest of the identifier
    while (std::isalnum(lexer->current_char) || lexer->current_char == '_') {
        if (length < MAX_LEXEME_LENGTH - 1) {
            lexeme[length++] = lexer->current_char;
            lex_advance(lexer);
        } else {
            std::cerr << "Error: Identifier too long" << std::endl;
            break;
        }
    }
    lexeme[length] = '\0';

    TokenType type = lookup_keyword(lexeme);
    return create_token(type, lexeme, lexer->line, start_column);
}

Token* lex_number(Lexer* lexer) {
    char lexeme[MAX_LEXEME_LENGTH] = {0};
    int length = 0;
    int start_column = lexer->column;

    while (std::isdigit(lexer->current_char)) {
        if (length < MAX_LEXEME_LENGTH - 1) {
            lexeme[length++] = lexer->current_char;
        }
        lex_advance(lexer);
    }
    lexeme[length] = '\0';

    return create_token(TOKEN_INT, lexeme, lexer->line, start_column);  // Changed TOKEN_INTEGER_LITERAL to TOKEN_INT
}

// Lex strings
Token* lex_string(Lexer* lexer) {
    char lexeme[MAX_LEXEME_LENGTH] = {0};
    int length = 0;
    int start_column = lexer->column;

    lex_advance(lexer);  // Skip opening quote
    while (lexer->current_char != '"' && lexer->current_char != EOF) {
        if (lexer->current_char == '\\') {  // Handle escape sequences
            lex_advance(lexer);
            switch (lexer->current_char) {
                case 'n': lexeme[length++] = '\n'; break;
                case 't': lexeme[length++] = '\t'; break;
                case 'r': lexeme[length++] = '\r'; break;
                case 'b': lexeme[length++] = '\b'; break;
                case 'f': lexeme[length++] = '\f'; break;
                case '0': lexeme[length++] = '\0'; break;
                case '"': lexeme[length++] = '"'; break;
                case '\\': lexeme[length++] = '\\'; break;
                default:
                    std::cerr << "Error: Unknown escape sequence '\\"
                              << lexer->current_char << "' at line "
                              << lexer->line << ", column " << lexer->column << std::endl;
                    lexeme[length++] = lexer->current_char; // Include the unknown escape sequence as-is
                    break;
            }
        } else if (length < MAX_LEXEME_LENGTH - 1) {
            lexeme[length++] = lexer->current_char;
        } else {
            std::cerr << "Error: String literal exceeds max length at line "
                      << lexer->line << ", column " << start_column << std::endl;
            break;
        }
        lex_advance(lexer);
    }

    if (lexer->current_char == '"') {
        lex_advance(lexer);  // Skip closing quote
        lexeme[length] = '\0';
        return create_token(TOKEN_STRING_LITERAL, lexeme, lexer->line, start_column);
    } else {
        std::cerr << "Error: Unterminated string literal at line "
                  << lexer->line << ", column " << start_column << std::endl;
        return create_token(TOKEN_ERROR, "Unterminated string", lexer->line, start_column);
    }
}

// Lex next token
Token* lex_next_token(Lexer* lexer) {
    if (!lexer || !lexer->file) {
        return nullptr;
    }

    skip_whitespace(lexer);

    if (lexer->current_char == EOF) {
        return create_token(TOKEN_EOF, "EOF", lexer->line, lexer->column);
    }

    // Handle comments
    if (lexer->current_char == '/') {
        int next_char = lexer->file->peek();
        if (next_char == '/' || next_char == '*') {
            skip_comments(lexer);
            return lex_next_token(lexer);
        }
    }

    int start_column = lexer->column;

    // Handle identifiers and keywords
    if (std::isalpha(lexer->current_char) || lexer->current_char == '_') {
        return lex_identifier_or_keyword(lexer);
    }

    // Handle numbers
    if (std::isdigit(lexer->current_char)) {
        return lex_number(lexer);
    }

    // Handle string literals
    if (lexer->current_char == '"') {
        return lex_string(lexer);
    }

    // Single-character tokens
    char current_char = lexer->current_char;
    Token* token = nullptr;

    switch (current_char) {
        case '+': 
            lex_advance(lexer); 
            token = create_token(TOKEN_PLUS, "+", lexer->line, start_column);
            break;
        case '-': 
            lex_advance(lexer); 
            token = create_token(TOKEN_MINUS, "-", lexer->line, start_column);
            break;
        case '*': 
            lex_advance(lexer); 
            token = create_token(TOKEN_ASTERISK, "*", lexer->line, start_column);
            break;
        case '/': 
            lex_advance(lexer); 
            token = create_token(TOKEN_SLASH, "/", lexer->line, start_column);
            break;
        case '%': 
            lex_advance(lexer); 
            token = create_token(TOKEN_PERCENT, "%", lexer->line, start_column);
            break;
        case '=': 
            lex_advance(lexer); 
            token = create_token(TOKEN_ASSIGN, "=", lexer->line, start_column);
            break;
        case '(': 
            lex_advance(lexer); 
            token = create_token(TOKEN_LPAREN, "(", lexer->line, start_column);
            break;
        case ')': 
            lex_advance(lexer); 
            token = create_token(TOKEN_RPAREN, ")", lexer->line, start_column);
            break;
        case '{': 
            lex_advance(lexer); 
            token = create_token(TOKEN_LBRACE, "{", lexer->line, start_column);
            break;
        case '}': 
            lex_advance(lexer); 
            token = create_token(TOKEN_RBRACE, "}", lexer->line, start_column);
            break;
        case '[': 
            lex_advance(lexer); 
            token = create_token(TOKEN_LBRACKET, "[", lexer->line, start_column);
            break;
        case ']': 
            lex_advance(lexer); 
            token = create_token(TOKEN_RBRACKET, "]", lexer->line, start_column);
            break;
        case ',': 
            lex_advance(lexer); 
            token = create_token(TOKEN_COMMA, ",", lexer->line, start_column);
            break;
        case ';': 
            lex_advance(lexer); 
            token = create_token(TOKEN_SEMICOLON, ";", lexer->line, start_column);
            break;
        default:
            std::cerr << "Error: Unrecognized character '" << current_char
                      << "' at line " << lexer->line << ", column " << lexer->column << std::endl;
            lex_advance(lexer);
            token = create_token(TOKEN_ERROR, "Unknown", lexer->line, start_column);
            break;
    }

    return token;
}
