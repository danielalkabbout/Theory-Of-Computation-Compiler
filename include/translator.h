#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include "parser.h"
#include <string>

bool translateToPython(ASTNode* root, std::string& outCode);

#endif // TRANSLATOR_H


