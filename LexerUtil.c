/* File: LexerUtil.c
 * Author: Michael Goulet
 * Implements LexerUtil.h
 */

#include <stdlib.h>
#include <string.h>
#include "LexerUtilities.h"
#include "ParserEnums.h"

void determineReservedLiteral(const char* string, ReservedLiteral* var) {
    switch (string[0]) {
        case 't':
            *var = RL_TRUE;
            break;
        case 'f':
            *var = RL_FALSE;
            break;
        case 'n':
            *var = RL_NULL;
            break;
        default:
            *var = -1;
            break;
    }
}

void determineOpType(const char* string, OperationType* var) {
    switch (string[0]) {
        case 'a':
            *var = OP_AND;
            break;
        case 'c':
            *var = OP_COMPL;
            break;
        case 'n':
            *var = OP_NOT;
            break;
        case 'o':
            *var = OP_OR;
            break;
        case '*':
            *var = OP_MULT;
            break;
        case '/':
            *var = OP_DIV;
            break;
        case '%':
            *var = OP_MOD;
            break;
        case '!':
            *var = OP_NOT_EQUALS;
            break;
        case '>':

            if (string[1] == '=')
                *var = OP_GRE_EQUALS;
            else
                *var = OP_GREATER;

            break;
        case '<':

            if (string[1] == '=')
                *var = OP_LES_EQUALS;
            else
                *var = OP_LESS;

            break;
        case '=':

            if (string[1] == '=')
                *var = OP_EQUALS;
            else
                *var = OP_SET;

            break;
        case '+':

            if (string[1] == '+')
                *var = OP_PLUSONE;
            else
                *var = OP_PLUS;

            break;
        case '-':

            if (string[1] == '-')
                *var = OP_MINUSONE;
            else
                *var = OP_MINUS;

            break;
    }
}

void saveIdentifier(const char* string, char** var) {
    int stringlen = strlen(string);
    char* cpystring = malloc(sizeof(char) * (stringlen+1));
    memcpy(cpystring, string, stringlen);
    cpystring[stringlen] = '\0';
    *var = cpystring;
}

void saveStringLiteral(const char* string, char** var) {
    int stringLength = strlen(string)-1;
    int interpretedLength = 0;
    int i;

    for (i = 1; i < stringLength; i++) {
        if (string[i] == '\\') {
            i++; //skip char after.
        }

        interpretedLength++;
    }

    char* newstring = malloc(interpretedLength + 1);
    newstring[interpretedLength] = '\0';
    int j; //i already defined.

    for (i = 1, j = 0; i < stringLength; i++) {
        if (string[i] == '\\') {
            i++; //skip char after.
            char escape = string[i], output;

            switch (escape) {
                case '\\':
                    output = '\\';
                    break;
                case '"':
                    output = '"';
                    break;
                case '\'':
                    output = '\'';
                    break;
                case 'b': //bell
                    output = '\b';
                    break;
                case 'f': //form-feed, new page
                    output = '\f';
                    break;
                case 'n': //new-line
                    output = '\n';
                    break;
                case 'r': //carriage return
                    output = '\r';
                    break;
                case 't': //tab
                    output = '\t';
                    break;
                default:
                    output = '\0';
                    break;
            }

            newstring[j] = escape;
        } else {
            newstring[j] = string[i];
        }

        j++;
    }

    *var = newstring;
}

char* saveIdentifierReturn(const char* string) {
    char* ret;
    saveIdentifier(string, &ret);
    return ret;
}