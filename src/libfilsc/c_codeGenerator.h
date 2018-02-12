/// <summary>
/// Generates 'C' language code from the checked program AST.
/// </summary>
/// <remarks>
/// It does not try to perform much optimization, as the 'C' compiler is suppossed 
/// to be good optimizing code.
/// </remarks>
#pragma once

#include "ast.h"
#include <string>

std::string generateCode(Ref<AstNode> node);