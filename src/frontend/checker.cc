enum class ScopeType{
    GLOBAL,
    PROC,
    BLOCK,
};

struct Scope;
struct VariableEntity{
    Type type;
    u64 size;
    u8 pointerDepth;
};
struct StructEntity{
    Scope *body;
    u64 size;
};

struct Scope{
    HashmapStr var;
    DynamicArray<VariableEntity> vars;
    HashmapStr struc;
    DynamicArray<StructEntity> strucs;
    ScopeType type;

    void init(ScopeType stype){
        type = stype;
        var.init();
        vars.init();
        struc.init();
        strucs.init();
    };
    void uninit(){
        strucs.uninit();
        struc.uninit();
        vars.uninit();
        var.uninit();
    };
};

static Scope *globalScopes;
static Scope *scopeAllocMem;
static u32 scopeOff;

VariableEntity *getVariableEntity(ASTBase *node, DynamicArray<Scope*> &scopes){
    String name;
    switch(node->type){
        case ASTType::VARIABLE:{
            ASTVariable *var = (ASTVariable*)node;
            name = var->name;
        }break;
        case ASTType::MODIFIER:{
            ASTModifier *mod = (ASTModifier*)node;
            name = mod->name;
        }break;
        default: return nullptr;
    }
    for(u32 x=scopes.count; x!=0;){
        x -= 1;
        Scope *scope = scopes[x];
        u32 off;
        if(!scope->var.getValue(name, &off)) continue;
        return &scope->vars[x];
    };
    return nullptr;
};
StructEntity *getStructEntity(ASTStruct *node, DynamicArray<Scope*> &scopes){
    for(u32 x=scopes.count; x!=0;){
        x -= 1;
        Scope *scope = scopes[x];
        u32 off;
        if(!scope->struc.getValue(node->name, &off)) continue;
        return &scope->strucs[x];
    };
    return nullptr;
};
VariableEntity *createVariableEntity(ASTBase *node, Scope *scope){
    String name;
    switch(node->type){
        case ASTType::VARIABLE:{
            ASTVariable *var = (ASTVariable*)node;
            name = var->name;
        }break;
        case ASTType::MODIFIER:{
            ASTModifier *mod = (ASTModifier*)node;
            name = mod->name;
        }break;
        default: return nullptr;
    }
    u32 id = scope->vars.count;
    scope->var.insertValue(name, id);
    return &scope->vars.newElem();
};

bool fillTypeInfo(Lexer &lexer, ASTTypeNode *node){
    BRING_TOKENS_TO_SCOPE;
    if(isType(tokTypes[node->tokenOff])){
        node->zType = (Type)((u32)tokTypes[node->tokenOff] - (u32)TokType::K_TYPE_START);
        return true;
    };
    if(tokTypes[node->tokenOff] != TokType::IDENTIFIER){
        lexer.emitErr(tokOffs[node->tokenOff].off, "Expected a type or a structure name");
        return false;
    };
    //TODO: structs
    return false;
};
Type checkTree(Lexer &lexer, ASTBase *node, DynamicArray<Scope*> &scopes, u32 &pointerDepth){
    BRING_TOKENS_TO_SCOPE;
    pointerDepth = 0;
    while(node->type > ASTType::U_START && node->type < ASTType::U_END){
        //unary ops return the type of the child
        ASTUnOp *unOp = (ASTUnOp*)node;
        node = unOp->child;
    };
    switch(node->type){
        case ASTType::BOOL:    return Type::BOOL;
        case ASTType::INTEGER: return Type::COMP_INTEGER;
        case ASTType::DECIMAL: return Type::COMP_DECIMAL;
        case ASTType::VARIABLE:{
            VariableEntity *entity = getVariableEntity(node, scopes);
            pointerDepth = entity->pointerDepth;
            return entity->type;
        }break;
        default:{
            if(node->type > ASTType::B_START && node->type < ASTType::B_END){
                ASTBinOp *binOp = (ASTBinOp*)node;
                u32 lhsUsingPointer, rhsUsingPointer;
                Type lhsType = checkTree(lexer, binOp->lhs, scopes, lhsUsingPointer);
                Type rhsType = checkTree(lexer, binOp->rhs, scopes, rhsUsingPointer);
                if(lhsUsingPointer && rhsUsingPointer){
                    lexer.emitErr(tokOffs[binOp->tokenOff].off, "Cannot perform binary operation with 2 pointers");
                    return Type::INVALID;
                };
                if(lhsType > Type::COUNT || rhsType > Type::COUNT){
                    lexer.emitErr(tokOffs[binOp->tokenOff].off, "Cannot perform binary operation with structures");
                    return Type::INVALID;
                };
                return (lhsType < rhsType)?lhsType:rhsType;
            };
        }break;
    };
    return Type::INVALID;
};
u64 getSize(Type type, DynamicArray<Scope*> &scopes){
    switch(type){
        case Type::S64:
        case Type::U64:  return 64;
        case Type::BOOL:
        case Type::S32:
        case Type::U32:  return 32;
        case Type::S16:
        case Type::U16:  return 16;
        case Type::CHAR:
        case Type::S8:
        case Type::U8:   return 8;
        default:{
            //TODO:
        }break;
    };
    return 0;
};
u64 checkAss(Lexer &lexer, ASTAssDecl *assdecl, DynamicArray<Scope*> &scopes){
    BRING_TOKENS_TO_SCOPE;
    u32 typePointerDepth;
    Type typeType;
    if(assdecl->zType){
        if(!fillTypeInfo(lexer, assdecl->zType)) return 0;
        ASTTypeNode *type = assdecl->zType;
        typeType = type->zType;
        typePointerDepth = type->pointerDepth;
    };
    if(assdecl->rhs){
        u32 treePointerDepth;
        Type treeType = checkTree(lexer, assdecl->rhs, scopes, treePointerDepth);
        if(treeType == Type::INVALID) return 0;
        if(treePointerDepth != typePointerDepth){
            lexer.emitErr(tokOffs[assdecl->tokenOff].off, "Expression tree pointer depth is not equal to type pointer depth");
            return 0;
        };
        if(treeType < typeType){
            lexer.emitErr(tokOffs[assdecl->tokenOff].off, "Explicit cast required");
            return 0;
        };
    };
    u64 size = getSize(typeType, scopes);
    for(u32 x=0; x<assdecl->lhsCount; x++){
        ASTBase *lhsNode = assdecl->lhs[x];
        if(getVariableEntity(lhsNode, scopes)){
            u32 off;
            switch(lhsNode->type){
                case ASTType::VARIABLE:{
                    ASTVariable *var = (ASTVariable*)lhsNode;
                    off = var->tokenOff;
                }break;
                case ASTType::MODIFIER:{
                    ASTModifier *mod = (ASTModifier*)lhsNode;
                    off = mod->tokenOff;
                }break;
                default: UNREACHABLE;
            }
            lexer.emitErr(tokOffs[off].off, "Redefinition");
            return 0;
        };
        VariableEntity *entity = createVariableEntity(lhsNode, scopes[scopes.count-1]);
        entity->pointerDepth = typePointerDepth;
        entity->type = typeType;
        if(typePointerDepth > 0) entity->size = 64;
        else entity->size = size;
    };
    return size;
}

bool checkScope(Lexer &lexer, ASTBase **nodes, u32 nodeCount, DynamicArray<Scope*> &scopes){
    BRING_TOKENS_TO_SCOPE;
    Scope *scope = scopes[scopes.count-1];
    for(u32 x=0; x<nodeCount; x++){
        ASTBase *node = nodes[x];
        switch(node->type){
            case ASTType::STRUCT:{
                ASTStruct *Struct = (ASTStruct*)node;
                if(getStructEntity(Struct, scopes)){
                    lexer.emitErr(tokOffs[Struct->tokenOff].off, "Structure already defined");
                    return false;
                };
                u32 id = scope->strucs.count;
                scope->struc.insertValue(Struct->name, id);
                StructEntity *entity = &scope->strucs.newElem();
                Scope *body = &scopeAllocMem[scopeOff++];
                body->init(ScopeType::BLOCK);
                entity->body = body;
                u64 size = 0;
                scopes.push(body);
                for(u32 x=0; x<Struct->bodyCount; x++){
                    ASTAssDecl *node = (ASTAssDecl*)Struct->body[x];
                    if(node->type != ASTType::DECLERATION){
                        lexer.emitErr(tokOffs[Struct->tokenOff].off, "Body should contain only declerations");
                        return false;
                    };
                    if(node->rhs){
                        lexer.emitErr(tokOffs[Struct->tokenOff].off, "Body should not contain decleration with RHS(expression tree)");
                        return false;
                    }
                    u64 temp = checkAss(lexer, node, scopes);
                    if(temp == 0) return false;
                    size += temp;
                };
                entity->size = size;
                scopes.pop();
            }break;
            case ASTType::DECLERATION:{
                return checkAss(lexer, (ASTAssDecl*)node, scopes)>0;
            }break;
            case ASTType::IF:{
                ASTIf *If = (ASTIf*)node;
                u32 treePointerDepth;
                Type treeType = checkTree(lexer, If->expr, scopes, treePointerDepth);
                if(treeType == Type::INVALID || (treeType > Type::COUNT && treePointerDepth == 0)){
                    lexer.emitErr(tokOffs[If->exprTokenOff].off, "Invalid expression");
                    return false;
                };
                Scope *bodyScope = &scopeAllocMem[scopeOff++];
                bodyScope->init(ScopeType::BLOCK);
                scopes.push(bodyScope);
                bool res = checkScope(lexer, If->ifBody, If->ifBodyCount, scopes);
                scopes.pop();
                return res;
            }break;
        };
    };
    return true;
};
bool checkASTFile(Lexer &lexer, ASTFile &file, Scope &scope){
    scope.init(ScopeType::GLOBAL);
    DynamicArray<Scope*> scopes;
    scopes.init();
    DEFER(scopes.uninit());
    for(u32 x=0; x<file.dependencies.count; x++){
        scopes.push(&globalScopes[file.dependencies[x]]);
    };
    scopes.push(&scope);
    return checkScope(lexer, file.nodes.mem, file.nodes.count, scopes);
};