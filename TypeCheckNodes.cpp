/*
 * File:   TypeSystem.cpp
 * Author: Michael Goulet
 * Implements: TypeSystem.h
 */

#define __GXX_EXPERIMENTAL_CXX0X__ 1

#include "Structures.h"
#include "TypeSystem.h"
#include "SyntaxTreeUtil.h"
#include <cmath>
#include <climits>

using std::floor;

#define WIDEN_NODE(newtype, oldtype, node) if (!equalTypes(newtype, oldtype)) { node = createCastOperation(node, newtype); }

/* This "method" is defined to implement the common "store-into-variable-and-widen-if-necessary"
 * method that is used for parameters, storing things into lval's, and also variable definitions.
 */
#define STORE_EXPRESSION_INTO_LVAL(ltype, rtype, node, reason) \
    if (!equalTypes(ltype, rtype)) { \
        if (isNumericalType(ltype) && isNumericalType(rtype)) { \
            CheshireType widetype = getWidestNumericalType(ltype, rtype); \
            WIDEN_NODE(widetype, ltype, node); \
        } else if (isObjectType(ltype) && isObjectType(rtype)) { \
            if (equalTypes(TYPE_NULL, rtype)) { \
                WIDEN_NODE(ltype, TYPE_NULL, node); \
            } else if (isUnsafe(ltype) ^ isUnsafe(rtype)) { \
                PANIC("Mixing safe and unsafe references!"); \
            } else if (isUnsafe(ltype) /*rtype must be unsafe too...*/) { \
                /* todo: make sure ltype >= rtype (up-cast), AND NULL CASE! */ \
            } \
        } else \
            PANIC("left and right types must be both numerical, object or the same exact type for %s", reason); \
    }

//////////////// STATICS /////////////////
extern KeyedLambdas keyedLambdas;
extern ObjectMapping objectMapping;
//////////////////////////////////////////

void typeCheckTopNode(CheshireScope* scope, ParserTopNode* node) {
    switch (node->type) {
        case PRT_NONE:
            PANIC("Type system received PRT_NONE.");
            break;
        case PRT_METHOD_DECLARATION:
            defineVariable(scope, node->method.functionName, getLambdaType(node->method.type, node->method.params));
            break;
        case PRT_METHOD_DEFINITION:
            setExpectedMethodType(scope, node->method.type);
            defineVariable(scope, node->method.functionName, getLambdaType(node->method.type, node->method.params));
            raiseScope(scope);

            for (ParameterList* p = node->method.params; p != NULL; p = p->next)
                defineVariable(scope, p->name, p->type);

            typeCheckBlockList(scope, node->method.body);
            fallScope(scope);
            break;
        case PRT_VARIABLE_DECLARATION:
            defineVariable(scope, node->variable.name, node->variable.type);
            break;
        case PRT_VARIABLE_DEFINITION: {
            CheshireType givenType = typeCheckExpressionNode(scope, node->variable.value);
            STORE_EXPRESSION_INTO_LVAL(node->variable.type, givenType, node->variable.value, "global variable");
            defineVariable(scope, node->variable.name, node->variable.type);
        }
        break;
        case PRT_CLASS_DEFINITION: {
            ERROR_IF(!isObjectType(node->classdef.parent) && !isUnsafe(node->classdef.parent), "Invalid parent type of class %s", node->classdef.name);
            int typekey = defineClass(node->classdef.name, node->classdef.classlist, node->classdef.parent);
            CStrEql streql;
            for (ClassList* c = node->classdef.classlist; c != NULL; c = c->next) {
                for (ClassList* next = c->next; next != NULL; next = next->next)
                    if (streql(c->name, next->name))
                        PANIC("Redeclaration of variable %s", c->name);
                if (c->type.typeKey == 0)
                    PANIC("Cannot have type void!");
            }
        }
        break;
    }
}

CheshireType typeCheckExpressionNode(CheshireScope* scope, ExpressionNode* node) {
    switch (node->type) {
        case OP_NOP:
            PANIC("No such operation as No-OP");
            return TYPE_VOID;
        case OP_NOT: {
            CheshireType childType = typeCheckExpressionNode(scope, node->unaryChild);

            if (isBoolean(childType)) {
                return TYPE_BOOLEAN;
            } else {
                PANIC("Expected type \"boolean\" for operation NOT");
            }

            return TYPE_BOOLEAN;
        }
        case OP_PLUSONE:
        case OP_MINUSONE: {
            CheshireType childType = typeCheckExpressionNode(scope, node->unaryChild);
            return childType;
        }
        case OP_COMPL:
        case OP_UNARY_MINUS: {
            CheshireType childType = typeCheckExpressionNode(scope, node->unaryChild);
            ERROR_IF(!isNumericalType(childType), "Expected a numerical type for operations COMPL and UNARY -");
            return childType;
        }
        case OP_GRE_EQUALS:
        case OP_LES_EQUALS:
        case OP_GREATER:
        case OP_LESS:
        case OP_EQUALS:
        case OP_NOT_EQUALS: {
            CheshireType left = typeCheckExpressionNode(scope, node->binary.left);
            CheshireType right = typeCheckExpressionNode(scope, node->binary.right);
            ERROR_IF(isVoid(left) || isVoid(right), "Cannot compare void type in operations >=, <=, >, <, !=, or ==");

            if (!equalTypes(left, right)) {
                if (isNumericalType(left) && isNumericalType(right)) {
                    CheshireType widetype = getWidestNumericalType(left, right);
                    WIDEN_NODE(widetype, left, node->binary.left);
                    WIDEN_NODE(widetype, right, node->binary.right);
                } else if ((node->type == OP_EQUALS || node->type == OP_NOT_EQUALS) && isObjectType(left) && isObjectType(right)) {
                    ERROR_IF(isUnsafe(left) ^ isUnsafe(right), "Mixing safe and unsafe references!");
                    //todo: make sure they are direct linege, AND NULL CASE!
                } else
                    PANIC("left and right types must be numerical for operations >=, <=, >, <, including object for == and !=");
            }

            return TYPE_BOOLEAN;
        }
        case OP_PLUS:
        case OP_MINUS:
        case OP_MULT:
        case OP_DIV:
        case OP_MOD: {
            CheshireType left = typeCheckExpressionNode(scope, node->binary.left);
            CheshireType right = typeCheckExpressionNode(scope, node->binary.right);

            if (isNumericalType(left) && isNumericalType(right)) {
                CheshireType widetype = getWidestNumericalType(left, right);
                WIDEN_NODE(widetype, left, node->binary.left);
                WIDEN_NODE(widetype, right, node->binary.right);
            } else
                PANIC("left and right types must be numerical for operations +, -, *, /, and %%");

            return left;
        }
        case OP_AND:
        case OP_OR: {
            CheshireType left = typeCheckExpressionNode(scope, node->binary.left);
            CheshireType right = typeCheckExpressionNode(scope, node->binary.right);
            ERROR_IF(!isBoolean(left) || !isBoolean(right), "Expected type Boolean in expression \"and\" or \"or\"");
            return TYPE_BOOLEAN;
        }
        case OP_SET: {
            CheshireType left = typeCheckExpressionNode(scope, node->binary.left);
            CheshireType right = typeCheckExpressionNode(scope, node->binary.right);
            STORE_EXPRESSION_INTO_LVAL(left, right, node->binary.right, "Operator =");
            return left;
        }
        case OP_ARRAY_ACCESS: {
            CheshireType left = typeCheckExpressionNode(scope, node->binary.left);
            CheshireType right = typeCheckExpressionNode(scope, node->binary.right);
            ERROR_IF(getArrayNesting(left) == 0, "Cannot dereference a non-array type!");
            ERROR_IF(isVoid(left), "No such dereference of type void[]!");
            ERROR_IF(!isInt(right), "Index of array must be an integer!");
            return getArrayDereference(left);
        }
        case OP_INSTANCEOF: {
            CheshireType child = typeCheckExpressionNode(scope, node->instanceof.expression);
            ERROR_IF(!isObjectType(child) || !isObjectType(node->instanceof.type), "Expected object types for operation instanceof");
            ERROR_IF(!isSuper(child, node->instanceof.type) || !isSuper(node->instanceof.type, child), "Instanceof may only compare type relationships that are possible!");
            return TYPE_BOOLEAN;
        }
        case OP_VARIABLE: {
            CheshireType ret = getVariableType(scope, node->string);
            return ret;
        }
        case OP_STRING:
            return TYPE_STRING;
        case OP_CAST: {
            CheshireType cast = node->cast.type;
            CheshireType child = typeCheckExpressionNode(scope, node->cast.child);

            if (isObjectType(cast) && equalTypes(TYPE_NULL, child)) //null case!
                return cast;

            ERROR_IF(isNumericalType(cast) ^ isNumericalType(child), "Received a mix of numerical and non-numerical types in cast!");
            ERROR_IF(isObjectType(cast) ^ isObjectType(child), "Received a mix of object and non-object types in cast!");
            ERROR_IF(isUnsafe(cast) ^ isUnsafe(child), "Received a mix of unsafe and managed objects in cast!");
            ERROR_IF(isLambdaType(cast) || isLambdaType(child), "Cannot operate on lambda types in cast!");
            return cast;
        }
        case OP_METHOD_CALL: {
            ExpressionList* expressions = node->methodcall.params;
            CheshireType functionType = getVariableType(scope, node->methodcall.fn_name);
            ERROR_IF(!isLambdaType(functionType), "Type is not invocable!");
            LambdaType method_signature = keyedLambdas[functionType];
            unsigned int index = 0;

            for (ExpressionList* paramNode = expressions; paramNode != NULL; paramNode = paramNode->next, index++) {
                ERROR_IF(index >= method_signature.second.size(), "Too many parameters for method call. Method takes %d parameters, more than %d parameters given!", method_signature.second.size(), index);
                CheshireType parameterExpectedType = method_signature.second[index];
                CheshireType parameterGivenType = typeCheckExpressionNode(scope, paramNode->parameter);
                STORE_EXPRESSION_INTO_LVAL(parameterExpectedType, parameterGivenType, paramNode->parameter, "parameter");
            }

            ERROR_IF(index != method_signature.second.size(), "Not enough parameters for method call!");
            return method_signature.first;
        }
        case OP_NUMBER: {
            double number = node->numberValue;

            if (number == floor(number)) {
                if (number >= INT_MIN && number <= INT_MAX) {
                    return TYPE_INT;
                }

                return TYPE_NUMBER;
            } else {
                return TYPE_DECIMAL;
            }

            return TYPE_VOID;
        }
        case OP_RESERVED_LITERAL: {
            switch (node->reserved) {
                case RL_TRUE:
                case RL_FALSE:
                    return TYPE_BOOLEAN;
                case RL_NULL:
                    return TYPE_NULL;
            }
        }
        case OP_LENGTH:
            return TYPE_INT;
        case OP_DEREFERENCE: {
            CheshireType child = typeCheckExpressionNode(scope, node->unaryChild);
            return child;
        }
        case OP_NEW_GC: {
            CheshireType object = node->instantiate.type;

            if (!isObjectType(object))
                PANIC("Impossible to instantate a non-object type!");

            object.isUnsafe = FALSE;
            return object;
        }
        case OP_NEW_HEAP: {
            CheshireType object = node->instantiate.type;

            if (!isObjectType(object))
                PANIC("Impossible to instantate a non-object type!");

            object.isUnsafe = TRUE;
            return object;
        }
        case OP_CLOSURE: {
            CheshireType currentExpectedType = getExpectedMethodType(scope);
            setExpectedMethodType(scope, node->closure.type);
            raiseScope(scope);
            
            for (ParameterList* p = node->closure.params; p != NULL; p = p->next)
                defineVariable(scope, p->name, p->type);
            
            typeCheckBlockList(scope, node->closure.body);
            fallScope(scope);
            setExpectedMethodType(scope, currentExpectedType);
            return getLambdaType(node->closure.type, node->closure.params);
        }
        case OP_ACCESS:
            CheshireType childType = typeCheckExpressionNode(scope, node->access.expression);
            ERROR_IF(!isObjectType(childType), "Cannot dereference a non-object type!");
            CStrEql streql;
            CheshireType ret = TYPE_VOID;
            
            for (ClassList* i = objectMapping[childType.typeKey]; i != NULL; i = i->next) {
                if (streql(i->name, node->access.variable))
                    ret = i->type;
            }
            
            if (equalTypes(ret, TYPE_VOID))
                PANIC("Could not find variable %s", node->access.variable);
            
            return ret;
    }

    return TYPE_VOID;
}

void typeCheckStatementNode(CheshireScope* scope, StatementNode* node) {
    switch (node->type) {
        case S_NOP:
            PANIC("No such statement as No-OP!");
            break;
        case S_VARIABLE_DEF: {
            CheshireType expectedType = node->varDefinition.type;
            CheshireType givenType = typeCheckExpressionNode(scope, node->varDefinition.value);
            defineVariable(scope, node->varDefinition.variable, expectedType);
            STORE_EXPRESSION_INTO_LVAL(expectedType, givenType, node->varDefinition.value, "variable definition");
        }
        break;
        case S_INFER_DEF: {
            CheshireType givenType = typeCheckExpressionNode(scope, node->varDefinition.value);
            defineVariable(scope, node->varDefinition.variable, givenType);
            node->varDefinition.type = givenType; //"infer" the type of the variable.
            printf("Inferred ");
            printCheshireType(givenType);
            printf(" for expression: ");
            printExpression(node->varDefinition.value);
            printf("\n");
        }
        break;
        case S_EXPRESSION: {
            typeCheckExpressionNode(scope, node->expression);
        }
        break;
        case S_ASSERT: {
            CheshireType givenType = typeCheckExpressionNode(scope, node->expression);

            if (!isBoolean(givenType))
                PANIC("Expected type boolean for assertion statement");
        }
        break;
        case S_BLOCK: {
            raiseScope(scope);
            typeCheckBlockList(scope, node->block);
            fallScope(scope);
        }
        break;
        case S_IF: {
            CheshireType condition = typeCheckExpressionNode(scope, node->expression);

            if (!isBoolean(condition))
                PANIC("Expected type boolean for if statement");

            raiseScope(scope);
            typeCheckStatementNode(scope, node->conditional.block);
            fallScope(scope);
        }
        break;
        case S_IF_ELSE: {
            CheshireType condition = typeCheckExpressionNode(scope, node->expression);

            if (!isBoolean(condition))
                PANIC("Expected type boolean for if-else statement");

            raiseScope(scope);
            typeCheckStatementNode(scope, node->conditional.block);
            fallScope(scope);
            raiseScope(scope);
            typeCheckStatementNode(scope, node->conditional.elseBlock);
            fallScope(scope);
        }
        break;
        case S_WHILE: {
            CheshireType condition = typeCheckExpressionNode(scope, node->expression);

            if (!isBoolean(condition))
                PANIC("Expected type boolean for while loop");

            raiseScope(scope);
            typeCheckStatementNode(scope, node->conditional.block);
            fallScope(scope);
        }
        break;
        case S_DELETE_HEAP: {
            CheshireType type = typeCheckExpressionNode(scope, node->expression);

            if (!type.isUnsafe)
                PANIC("Expected unsafe type in operator delete^");
        }
        break;
        case S_RETURN: {
            CheshireType type = typeCheckExpressionNode(scope, node->expression);
            CheshireType expected = getExpectedMethodType(scope);

            if (equalTypes(expected, TYPE_VOID)) {
                if (!equalTypes(type, TYPE_NULL))
                    PANIC("Cannot return void from a non-void method!");
            } else {
                STORE_EXPRESSION_INTO_LVAL(expected, type, node->expression, "return statement");
            }
        }
        break;
    }
}

void typeCheckBlockList(CheshireScope* scope, BlockList* list) {
    if (list == NULL)
        return;

    typeCheckStatementNode(scope, list->statement);
    typeCheckBlockList(scope, list->next);
}
