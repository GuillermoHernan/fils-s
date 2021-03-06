/// <summary>
/// Generates 'C' language code from the checked program AST.
/// </summary>
/// <remarks>
/// It does not try to perform much optimization, as the 'C' compiler is suppossed 
/// to be good optimizing code.
/// </remarks>

#include "pch.h"
#include "c_codeGenerator_internal.h"
#include "compileError.h"
#include "utils.h"
#include "codeGeneratorState.h"

using namespace std;

/// <summary>
/// 'C' code generation entry point. Generates 'C' source from the AST.
/// </summary>
/// <param name="node">AST root</param>
/// <returns>A string containing 'C' source code.</returns>
string generateCode(Ref<AstNode> node)
{
    static const CodeGeneratorConfig defaultCfg;

    return generateCode(node, defaultCfg);
}

/// <summary>
/// 'C' code generation entry point. Generates 'C' source from the AST.
/// It allows to specify the entry point.
/// </summary>
/// <param name="node">AST root</param>
/// <param name="config">Code generator configuration</param>
/// <returns></returns>
string generateCode(Ref<AstNode> node, const CodeGeneratorConfig& config)
{
    ostringstream		output;
    CodeGeneratorState	state(&output);

    //Set names for items which have defaults.
    auto &topLevelItems = node->children();

    for (auto& item : topLevelItems)
    {
        auto it = config.predefNames.find(item->getName());

        if (it != config.predefNames.end())
            state.setCname(item, it->second);
    }

    //write prolog.
    state.output() << config.prolog;

    //Generate types.
    auto types = astGatherTypes(node);
    for (auto& type : types)
        dataTypeCodegen(type, state);

    //Get functions
    auto functions = astGatherFunctions(node.getPointer());

    //Declare functions.
    for (auto& fn : functions)
        declareFunction(fn, state);

    state.output() << "\n\n";

    //Generate functions code.
    for (auto& fn : functions)
        codegen(fn, state, VoidVariable());

    //Actors code generation.
    auto actors = astGatherActors(node.getPointer());
    for (auto& actor : actors)
        codegen(actor, state, VoidVariable());

    //Write epilog
    state.output() << config.epilog;

    return output.str();
}

/// <summary>
/// Generates code for an AST node. Based on the node type, selects the appropriate 
/// code generation function.
/// </summary>
/// <remarks>
/// When adding new AST nodes, the table in this function shall be updated.
/// </remarks>
/// <param name="node"></param>
/// <param name="output"></param>
/// <param name="state"></param>
/// <param name="resultDest">Name of the 'C' variable where the result of the expression
/// should be stored.
/// </param>
void codegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    static NodeCodegenFN types[AST_TYPES_COUNT] = { NULL, NULL };

    if (types[0] == NULL)
    {
        //Default everything to 'invalid node'. To gracefully handle the bugs caused
        //by not keeping this list updated ;-)
        fill_n(types, AST_TYPES_COUNT, invalidNodeCodegen);

        types[AST_MODULE] = moduleCodegen;
        types[AST_SCRIPT] = nodeListCodegen;
        types[AST_TYPEDEF] = voidCodegen;
        types[AST_LIST] = invalidNodeCodegen;
        types[AST_BLOCK] = blockCodegen;
        types[AST_TUPLE] = tupleCodegen;
        types[AST_DECLARATION] = varCodegen;
        types[AST_TUPLE_DEF] = tupleDefCodegen;
        types[AST_TUPLE_ADAPTER] = tupleAdapterCodegen;
        types[AST_IF] = ifCodegen;
        types[AST_FOR] = invalidNodeCodegen;
        types[AST_FOR_EACH] = invalidNodeCodegen;
        types[AST_RETURN] = returnCodegen;
        types[AST_FUNCTION] = functionCodegen;
        types[AST_ASSIGNMENT] = assignmentCodegen;
        types[AST_FNCALL] = callCodegen;
        types[AST_CTCALL] = arrayAccessOpCodegen;
        types[AST_INTEGER] = literalCodegen;
        types[AST_FLOAT] = literalCodegen;
        types[AST_STRING] = literalCodegen;
        types[AST_BOOL] = literalCodegen;
        types[AST_IDENTIFIER] = varAccessCodegen;
        types[AST_ARRAY] = invalidNodeCodegen;
        types[AST_MEMBER_ACCESS] = memberAccessCodegen;
        types[AST_BINARYOP] = binaryOpCodegen;
        types[AST_PREFIXOP] = prefixOpCodegen;
        types[AST_POSTFIXOP] = postfixOpCodegen;
        types[AST_ACTOR] = actorCodegen;
        types[AST_OUTPUT] = outputMessageCodegen;
        types[AST_DEFAULT_TYPE] = invalidNodeCodegen;
        types[AST_TYPE_NAME] = voidCodegen;
        types[AST_IMPORT] = voidCodegen;
        types[AST_GET_ADDRESS] = getAddressCodegen;
    }

    if (node.notNull())
        types[node->getType()](node, state, resultDest);
}

/// <summary>
/// Generates code for a data type.
/// </summary>
/// <param name="type"></param>
/// <param name="state"></param>
void dataTypeCodegen(AstNode* type, CodeGeneratorState& state)
{
    switch (type->getType())
    {
    //case AST_TUPLE:
    case AST_TUPLE_DEF:
        tupleDefCodegen(type, state);
        break;

    case AST_ARRAY_DECL:
        arrayTypeCodegen(type, state);
        break;

    case AST_ACTOR:
        generateActorStruct(type, state);
        break;

    default:
        //The default behaviour is to not generate nothing.
        break;
    }
}


/// <summary>
/// Code generation function for nodes which do not require code generation
/// </summary>
void voidCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
}

/// <summary>
/// Handles the case of node types which are not suppossed to reach code generation phase.
/// </summary>
void invalidNodeCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    errorAt(node->position(),
        ETYPE_INVALID_CODEGEN_NODE_1,
        astTypeToString(node->getType()).c_str());
}

/// <summary>
/// Calls code generation for all children of the node.
/// </summary>
void nodeListCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    assert(resultDest.isVoid());

    for (auto child : node->children())
        codegen(child, state, VoidVariable());
}

/// <summary>
/// Module code generation. Calls code generation for all the children 'AST_SCRIPT' nodes.
/// </summary>
void moduleCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    assert(resultDest.isVoid());

    for (auto child : node->children())
    {
        if (child.notNull() && child->getType() == AST_SCRIPT)
            codegen(child, state, VoidVariable());
    }
}


/// <summary>
/// Declares a function, so it can be used by the code below.
/// </summary>
void declareFunction(AstNode* node, CodeGeneratorState& state)
{
    state.output() << genFunctionHeader(node, state);
    state.output() << ";\n";
}

/// <summary>
/// Generates code for a function definition node.
/// </summary>
void functionCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    if (node->hasFlag(ASTF_EXTERN_C))
        return;

    //Necessary because functions usually define their own temporaries.
    CodegenBlock	functionBlock(state);

    //Generate code for the parameters tuple.
    auto fnCode = astGetFunctionBody(node.getPointer());
    auto returnType = astGetReturnType(node->getDataType());

    state.output() << "//Code for '" << node->getName() << "' function\n";
    state.output() << genFunctionHeader(node, state);
    state.output() << "{\n";

    if (astIsVoidType(returnType))
        codegen(fnCode, state, VoidVariable());
    else
    {
        TempVariable	tmpReturn(returnType, state, false);
        codegen(fnCode, state, tmpReturn);

        state.output() << "return " << tmpReturn.cname() << ";\n";
    }

    state.output() << "}\n\n";
}

/// <summary>
/// Generates the 'C' header of a function.
/// </summary>
string genFunctionHeader(Ref<AstNode> node, CodeGeneratorState& state)
{
    auto	params = node->child(0);
    auto	type = node->getDataType();
    auto	retType = astGetReturnType(type);
    string	result;

    result.reserve(128);

    if (!node->hasFlag(ASTF_EXTERN_C))
        result = "static ";

    //Return type.
    if (astIsVoidType(retType))
        result += "void ";
    else
        result += state.cname(retType) + " ";

    //Function name
    result += state.cname(node);

    //Parameters.
    if (params->childCount() == 0)
        result += "()";
    else
        result += "(" + state.cname(params) + "* _gen_params)";

    return result;
}

/// <summary>
/// Generates the 'C' header of an input message.
/// </summary>
std::string genInputMsgHeader(
    Ref<AstNode> actor,
    Ref<AstNode> input,
    CodeGeneratorState& state,
    const std::string& nameOverride
)
{
    const string actorCName = state.cname(actor);
    string fnCName = nameOverride;
    auto params = astGetParameters(input.getPointer());
    string result;

    if (nameOverride == "")
        fnCName = state.cname(input);

    //Header
    result = "static void " + fnCName;
    result += "(" + actorCName + "* _gen_actor";

    if (params->childCount() == 0)
        result += ", const void* _no_params)";
    else
        result += ", " + state.cname(params) + "* _gen_params)";

    return result;
}

/// <summary>
/// Generates a function params structure.
/// </summary>
/// <param name="node">Function node. First child must be the parameters tuple</param>
/// <param name="state"></param>
/// <param name="commentSufix">Text added to the end of the structure header comment</param>
void generateParamsStruct(Ref<AstNode> node, CodeGeneratorState& state, const std::string& commentSufix)
{
    auto params = node->child(0);
    if (params->childCount() > 0)
    {
        state.output() << "//Parameters for '" << node->getName() << "' " << commentSufix << "\n";
        codegen(params, state, VoidVariable());
    }
}



/// <summary>
/// Generates code for a block of expressions
/// </summary>
void blockCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    if (node->childCount() == 0)
        return;

    const auto &	children = node->children();
    auto			lastChild = children.back();

    CodegenBlock	block(state);

    state.output() << "{\n";

    for (size_t i = 0; i < children.size() - 1; ++i)
        codegen(children[i], state, VoidVariable());

    codegen(lastChild, state, resultDest);

    state.output() << "}\n";
}

/// <summary>
/// Generates code for a tuple creation expression.
/// </summary>
void tupleCodegen(
    Ref<AstNode> node,
    CodeGeneratorState& state,
    const IVariableInfo& resultDest)
{
    if (resultDest.isVoid())
        return;

    auto&	expressions = node->children();

    for (size_t i = 0; i < expressions.size(); ++i)
    {
        TupleField	field(resultDest, i, state);

        codegen(expressions[i], state, field);
    }
}

/// <summary>
/// Generates code for a variable declaration.
/// </summary>
void varCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    auto	typeNode = node->getDataType();

    state.output() << state.cname(typeNode) << " ";
    state.output() << state.cname(node) << ";\n";

    if (node->childExists(1))
        codegen(node->child(1), state, NamedVariable(node, state));
}

/// <summary>
/// Generates code for a tuple definition.
/// </summary>
void tupleDefCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    string name = state.cname(node);

    state.output() << "typedef struct {\n";

    nodeListCodegen(node, state, VoidVariable());

    state.output() << "}" << name << ";\n\n";
}

/// <summary>
/// Generates code for a tuple definition.
/// Generates it from a data type, instead of an AST node. For not declared, inferred types.
/// </summary>
void tupleDefCodegen(AstNode* type, CodeGeneratorState& state)
{
    assert(astIsTupleType(type));

    if (type->childCount() == 0)
        return;		//Empty tuples shall not be generated

    string name = state.cname(type);

    state.output() << "typedef struct {\n";

    const int count = type->childCount();
    for (int i = 0; i < count; ++i)
    {
        auto child = type->child(i);

        state.output() << state.cname(child->getDataType()) << " ";
        state.output() << state.cname(child) << ";\n";
    }

    state.output() << "}" << name << ";\n\n";
}

/// <summary>
/// Declares an array type
/// </summary>
/// <param name="type"></param>
/// <param name="state"></param>
void arrayTypeCodegen(AstNode* type, CodeGeneratorState& state)
{
    auto    itemType = type->child(0)->getDataType();
    string  name = state.cname(type);
    string  itemTypeName = state.cname(itemType);
    string  size = type->child(1)->getValue();

    state.output() << "typedef " << itemTypeName << " " << name << "[" << size << "];\n";
}

/// <summary>
/// Generates code for a tuple adapter node.
/// </summary>
void tupleAdapterCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    assert(!resultDest.isVoid());

    //TODO: This is not going to work when default tuple values are implemented.
    //(or other more complex type-adapting features)
    TempVariable	rTemp(node->child(0), state, false);
    string			lName = resultDest.cname();

    codegen(node->child(0), state, rTemp);
    state.output() << "memcpy (&" << lName << ", &" << rTemp.cname()
        << ", sizeof(" << resultDest.cname() << "));\n";

}


/// <summary>
/// Generates code for an 'if' flow control expression.
/// </summary>
void ifCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    auto	condition = node->child(0);
    auto	thenExpr = node->child(1);
    auto	elseExpr = node->child(2);

    //Condition
    TempVariable conditionTempVar(condition, state, false);

    codegen(condition, state, conditionTempVar);

    state.output() << "if(" << conditionTempVar.cname() << "){\n";

    //Then
    codegen(thenExpr, state, resultDest);
    state.output() << "}\n";

    //Else (optional)
    if (elseExpr.notNull())
    {
        state.output() << "else{\n";
        codegen(elseExpr, state, resultDest);
        state.output() << "}\n";
    }
}

/// <summary>
/// Generates code for a return statement
/// </summary>
void returnCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    if (!node->childExists(0))
        state.output() << "return;\n";
    else
    {
        auto			expression = node->child(0);
        TempVariable	tempVar(node, state, false);

        codegen(expression, state, tempVar);
        state.output() << "return " << tempVar.cname() << ";\n";
    }
}

/// <summary>
/// Generates code for an assignment expression.
/// </summary>
void assignmentCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    auto	lexpr = node->child(0);
    auto	rexpr = node->child(1);

    assert(node->getValue() == "=");

    TempVariable	lRef(lexpr, state, true);
    TempVariable	rResult(rexpr, state, false);

    codegen(lexpr, state, lRef);
    codegen(rexpr, state, rResult);

    state.output() << "*" << lRef << " = " << rResult << ";\n";
    if (!resultDest.isVoid())
        state.output() << resultDest << " = " << rResult << ";\n";
}

/// <summary>
/// Generates code for a function call expression.
/// </summary>
void callCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    auto fnExpr = node->child(0);
    auto paramsExpr = node->child(1);

    //By the moment, only direct function invocation is supported
    assert(fnExpr->getType() == AST_IDENTIFIER);

    auto	fnNode = fnExpr->getReference();
    auto    fnType = fnNode->getDataType();
    string	fnCName = state.cname(fnNode);

    //TODO: Split in several functions.
    if (fnType->getType() == AST_MESSAGE_TYPE)
    {
        assert(resultDest.isVoid());
        TempVariable    addressTemp(fnType, state, false);

        codegen(fnExpr, state, addressTemp);
        if (paramsExpr->childCount() == 0)
        {
            state.output() << "postMessage (&" << addressTemp << ", NULL, 0);\n";
        }
        else
        {
            auto			paramsType = astGetParameters(fnType);
            TempVariable	tmpParams(paramsType, state, false);

            codegen(paramsExpr, state, tmpParams);
            state.output() << "postMessage (&" << addressTemp << ", &" << tmpParams;
            state.output() << ", sizeof(" << tmpParams << ")); \n";
        }
    }
    else if (fnType->getType() == AST_ACTOR)
    {
        if (paramsExpr->childCount() == 0)
        {
            state.output() << state.cname(fnType) + "_constructor (&" << resultDest << ", NULL);\n";
        }
        else
        {
            auto			paramsType = astGetParameters(fnType);
            TempVariable	tmpParams(paramsType, state, false);

            codegen(paramsExpr, state, tmpParams);
            state.output() << state.cname(fnType) + "_constructor (&" << resultDest 
                << ", &" << tmpParams << ");\n";
        }
    }
    else
    {
        if (paramsExpr->childCount() == 0)
        {
            assert(resultDest.isVoid());

            state.output() << fnCName << "();\n";
        }
        else
        {
            auto			paramsType = astGetParameters(fnType);
            TempVariable	tmpParams(paramsType, state, false);

            codegen(paramsExpr, state, tmpParams);

            if (!resultDest.isVoid())
                state.output() << resultDest.cname() << " = ";

            state.output() << fnCName << "(&" << tmpParams.cname() << ");\n";
        }

    }
}

/// <summary>
/// Generates code for the intem access operator '[]' 
/// </summary>
void arrayAccessOpCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    //TODO: Array references should be checked!!! This is supposed to be one of the main 
    //features of this language.
    auto arrayExpr = node->child(0);
    auto indexExpr = node->child(1)->child(0);
    auto arrayItemType = node->getDataType();

    TempVariable    tmpArray(arrayItemType, state, true);
    TempVariable    indexTmp(indexExpr->getDataType(), state, false);

    //HACK: The 'array' variable is treated as a reference when declared, but as 
    //not a reference when used. This is due how arrays in 'C' are treated.
    tmpArray.isReference = false;

    codegen(arrayExpr, state, tmpArray);
    codegen(indexExpr, state, indexTmp);

    string          refPrefix;
    if (resultDest.isReference)
        refPrefix = "&";

    state.output() << resultDest << " = " << refPrefix << tmpArray << "[" << indexTmp << "];\n";
}

/// <summary>
/// Generates code for a literal node
/// </summary>
void literalCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    if (resultDest.isVoid())
        return;

    switch (node->getType())
    {
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_BOOL:
        state.output() << resultDest.cname() << " = " << node->getValue() << ";\n";
        break;

    case AST_STRING:
        state.output() << resultDest.cname() << " = " << escapeString(node->getValue(), true) << ";\n";
        break;

    default:
        assert(!"Unexpected literal type");
    }
}

/// <summary>
/// Generates code to read a variable.
/// </summary>
void varAccessCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    if (resultDest.isVoid())
        return;

    if (node->getReference()->getType() == AST_INPUT)
    {
        assert(!resultDest.isReference);
        state.output() << resultDest.cname() << ".actorPtr = _gen_actor;\n";
        state.output() << resultDest.cname() << ".inputPtr = " << state.cname(node->getReference()) << ";\n";
    }
    else
    {
        string accessExpression = varAccessExpression(node, state);
        if (resultDest.isReference)
            state.output() << resultDest.cname() << " = &" << accessExpression << ";\n";
        else
            state.output() << resultDest.cname() << " = " << accessExpression << ";\n";
    }
}


/// <summary>
/// Generates code for an access to member expression.
/// </summary>
void memberAccessCodegen(
    Ref<AstNode> node,
    CodeGeneratorState& state,
    const IVariableInfo& resultDest)
{
    if (resultDest.isVoid())
        return;

    auto ltype = node->child(0)->getDataType();

    switch (ltype->getType())
    {
    //case AST_TUPLE:
    case AST_TUPLE_DEF:
        tupleMemberAccessCodegen(node, state, resultDest);
        return;

    case AST_ACTOR:
        actorMemberAccessCodegen(node, state, resultDest);
        return;

    default:
        break;      //Will throw an error.
    }//switch

    string msg = "Invalid left expression data type on member access: ";
    msg += astTypeToString(ltype);
    errorAt(node->position(),
        ETYPE_CODE_GENERATION_ERROR_1,
        msg.c_str());
}

/// <summary>
/// Member access code generation for tuples.
/// </summary>
void tupleMemberAccessCodegen(
    Ref<AstNode> node,
    CodeGeneratorState& state,
    const IVariableInfo& resultDest)
{
    auto lexpr = node->child(0);
    auto rnode = node->child(1);
    auto ltype = lexpr->getDataType();

    bool refVariable = (lexpr->getType() == AST_MEMBER_ACCESS || lexpr->getType() == AST_IDENTIFIER);

    TempVariable lexprResult(ltype, state, refVariable);
    codegen(lexpr, state, lexprResult);

    string  fieldName = rnode->getName();
    int     index = astFindMemberByName(ltype, fieldName);

    if (index < 0)
    {
        //This should not happen
        //TODO: Just an assert?
        errorAt(node->position(),
            ETYPE_MEMBER_NOT_FOUND_2,
            fieldName.c_str(),
            astTypeToString(ltype).c_str());
    }

    string fieldCName = state.cname(ltype->child(index));

    state.output() << resultDest << " = ";
    if (resultDest.isReference)
        state.output() << "&";
    
    const char* accessOp = refVariable ? "->" : ".";
    state.output() << lexprResult << accessOp << fieldCName << ";\n";
}

/// <summary>
/// Member access code generation for actors.
/// It is used to access message endpoints. In the future, to access sockets.
/// </summary>
void actorMemberAccessCodegen(
    Ref<AstNode> node,
    CodeGeneratorState& state,
    const IVariableInfo& resultDest)
{
    assert(!resultDest.isReference);
    
    auto lexpr = node->child(0);
    auto rnode = node->child(1);
    auto ltype = lexpr->getDataType();

    TempVariable lexprResult(ltype, state, true);
    codegen(lexpr, state, lexprResult);

    state.output() << resultDest << ".actorPtr = " << lexprResult << ";\n";
    state.output() << resultDest << ".inputPtr = (void*)" << state.cname(rnode) << ";\n";
}

/// <summary>
/// Generates code for a binary operation
/// </summary>
void binaryOpCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    if (resultDest.isVoid())
        return;

    auto leftExpr = node->child(0);
    auto rightExpr = node->child(1);
    auto operation = node->getValue();

    TempVariable	leftTmp(leftExpr, state, false);
    TempVariable	rightTmp(rightExpr, state, false);

    codegen(leftExpr, state, leftTmp);
    codegen(rightExpr, state, rightTmp);

    state.output() << resultDest << " = ";
    state.output() << leftTmp.cname() << operation << rightTmp.cname() << ";\n";
}


/// <summary>
/// Generates code for a prefix operator
/// </summary>
void prefixOpCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    auto child = node->child(0);
    auto operation = node->getValue();
    bool needsRef = (operation == "++" || operation == "--");

    TempVariable	temp(child, state, needsRef);

    codegen(child, state, temp);

    if (!resultDest.isVoid())
        state.output() << resultDest << " = ";

    state.output() << operation;
    if (needsRef)
        state.output() << "*";
    state.output() << temp << ";\n";
}


/// <summary>
/// Generates code for a postfix operator
/// </summary>
void postfixOpCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    auto child = node->child(0);
    auto operation = node->getValue();

    TempVariable	temp(child, state, true);

    codegen(child, state, temp);

    if (!resultDest.isVoid())
        state.output() << resultDest << " = ";
    state.output() << "(*" << temp << ")" << operation << ";\n";
}

/// <summary>
/// Generates code for the operation to get the address of a variable.
/// </summary>
void getAddressCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    auto            child = node->child(0);
    TempVariable	temp(child, state, true);

    codegen(child, state, temp);

    state.output() << resultDest << " = " << temp << ";\n";
}


/// <summary>
/// Generates the code associated to an actor definition.
/// </summary>
void actorCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    //generateActorStruct(node, state);
    generateActorInputs(node, state);
    generateActorConstructor(node, state);
}

/// <summary>
/// Generates code for an output message.
/// It defines a 'MessageSlot' in which an input message can be registered.
/// </summary>
void outputMessageCodegen(Ref<AstNode> node, CodeGeneratorState& state, const IVariableInfo& resultDest)
{
    string name = state.cname(node);

    state.output() << "MessageSlot " << name << ";\n";
}


/// <summary>
/// Generates the data structure which contains the actor data.
/// </summary>
void generateActorStruct(AstNode* type, CodeGeneratorState& state)
{
    string name = state.cname(type);

    state.output() << "typedef struct " << "{\n";

    auto params = astGetParameters(type);

    if (params->childCount() > 0)
    {
        string paramsTypeName = state.cname(params);
        state.output() << paramsTypeName << " params;\n";
    }

    for (size_t i = 1; i < type->childCount(); ++i)
    {
        auto child = type->child(i);
        if (child->getType() == AST_DECLARATION)
        {
            string childName = state.cname(child);
            string childTypeName = state.cname(child->getDataType());

            state.output() << childTypeName << " " << childName << ";\n";
        }
        else if (child->getType() == AST_OUTPUT)
        {
            string childName = state.cname(child);

            state.output() << "MessageSlot " << childName << ";\n";
        }
    }

    state.output() << "}" << name << ";\n\n";
}

/// <summary>
/// Generates the actor constructor function.
/// </summary>
/// <param name="node"></param>
/// <param name="state"></param>
void generateActorConstructor(Ref<AstNode> node, CodeGeneratorState& state)
{
    string actorCName = state.cname(node);
    string fnCName = actorCName + "_constructor";

    //Necessary because initialization expression may require temporaries.
    CodegenBlock	functionBlock(state);

    //generateParamsStruct(node, state, "actor");

    //Header
    state.output() << "//Code for '" << node->getName() << "' actor constructor\n";
    state.output() << genInputMsgHeader(node, node, state, fnCName) << "{\n";

    //Copy parameters
    auto params = astGetParameters(node.getPointer());

    if (params->childCount() > 0)
        state.output() << "_gen_actor->params = *_gen_params;\n";

    state.output() << "\n";

    //Initialice members
    const size_t count = node->childCount();
    for (size_t i = 1; i < count; ++i)
    {
        auto child = node->child(i);
        auto childType = child->getType();

        if (childType == AST_DECLARATION)
        {
            NamedVariable memberVar(child, state);

            codegen(child->child(1), state, memberVar);
        }
        else if (childType == AST_UNNAMED_INPUT)
        {
            generateConnection(node, child, state);
        }
    }

    state.output() << "}\n\n";
}

/// <summary>
/// Generates the code for actor inputs (named and unnamed)
/// </summary>
/// <param name="node"></param>
/// <param name="state"></param>
void generateActorInputs(Ref<AstNode> node, CodeGeneratorState& state)
{
    for (auto child : node->children())
    {
        auto type = child->getType();
        if (type == AST_INPUT || type == AST_UNNAMED_INPUT)
            generateActorInput(node, child, state);
    }
}

/// <summary>
/// Generates code for an actor input (named and unnamed)
/// </summary>
void generateActorInput(Ref<AstNode> actor, Ref<AstNode> input, CodeGeneratorState& state)
{
    string actorCName = state.cname(actor);
    string fnCName = state.cname(input);

    //Declare a block for temporaries.
    CodegenBlock	functionBlock(state);

    //Header
    state.output() << "//Code for '" << input->getName() << "' input message\n";
    state.output() << genInputMsgHeader(actor, input, state) << "{\n";

    codegen(astGetFunctionBody(input.getPointer()), state, VoidVariable());

    state.output() << "}\n\n";
}

/// <summary>
/// Generates the code which connects and output to an input.
/// </summary>
void generateConnection(Ref<AstNode> actor, Ref<AstNode> connection, CodeGeneratorState& state)
{
    vector<string>	path;
    auto			type = actor->getDataType();

    for (auto pathNode : connection->child(0)->children())
    {
        const int index = astFindMemberByName(type, pathNode->getName());
        assert(index >= 0);
        auto child = type->child(index);

        path.push_back(state.cname(child));

        type = child->getDataType();
    }

    string strPath = "_gen_actor->" + join(path, ".");

    state.output() << strPath << ".actorPtr = (void*)_gen_actor;\n";
    state.output() << strPath << ".inputPtr = (void*)" << state.cname(connection) << ";\n";
}



/// <summary>
/// Generates the expression need to access a variable. 
/// It returns it, it does not write it on the output
/// </summary>
/// <param name="node"></param>
/// <param name="state"></param>
/// <returns></returns>
std::string varAccessExpression(Ref<AstNode> node, CodeGeneratorState& state)
{
    string namePrefix = "";

    auto referenced = node->getReference();

    if (referenced->hasFlag(ASTF_ACTOR_MEMBER))
    {
        if (referenced->hasFlag(ASTF_FUNCTION_PARAMETER))
            namePrefix = "_gen_actor->params.";
        else
            namePrefix = "_gen_actor->";
    }
    else if (referenced->hasFlag(ASTF_FUNCTION_PARAMETER))
        namePrefix = "_gen_params->";

    return namePrefix + state.cname(referenced);
}
