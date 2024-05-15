enum class ScopeType{
    GLOBAL,
    PROC,
};

struct VariableEntity{
    Type type;
    u8 pointerDepth;
};

struct Scope{
    HashmapStr var;
    DynamicArray<VariableEntity> vars;
    ScopeType type;

    void init(ScopeType stype){
        type = stype;
        var.init();
        vars.init();
    };
    void uninit(){
        vars.uninit();
        var.uninit();
    };
};

static Scope *globalScopes;

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
    for(u32 x=scopes.count-1; x>0;){
        x -= 1;
        Scope *scope = scopes[x];
        u32 off;
        if(!scope->var.getValue(name, &off)){
            x -= 1;
            continue;
        };
        return &scope->vars[x];
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
    switch(node->type){
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
                if(lhsType > Type::TYPE_COUNT || rhsType > Type::TYPE_COUNT){
                    lexer.emitErr(tokOffs[binOp->tokenOff].off, "Cannot perform binary operation with structures");
                    return Type::INVALID;
                };
                return (lhsType < rhsType)?lhsType:rhsType;
            };
        }break;
    };
    return Type::INVALID;
};

bool checkScope(Lexer &lexer, ASTFile &file, DynamicArray<Scope*> &scopes){
    BRING_TOKENS_TO_SCOPE;
    Scope *scope = scopes[scopes.count-1];
    for(u32 x=0; x<file.nodes.count; x++){
        ASTBase *node = file.nodes[x];
        switch(node->type){
            case ASTType::DECLERATION:{
                ASTAssDecl *assdecl = (ASTAssDecl*)node;
                u32 treePointerDepth;
                Type treeType = checkTree(lexer, assdecl->rhs, scopes, treePointerDepth);
                if(treeType == Type::INVALID) return false;
                if(assdecl->zType){
                    if(!fillTypeInfo(lexer, assdecl->zType)) return false;
                    ASTTypeNode *type = assdecl->zType;
                    if(treePointerDepth != type->pointerDepth){
                        lexer.emitErr(tokOffs[assdecl->tokenOff].off, "Expression tree pointer depth is not equal to type pointer depth");
                        return false;
                    };
                    if(treeType < type->zType){
                        lexer.emitErr(tokOffs[assdecl->tokenOff].off, "Explicit cast required");
                        return false;
                    };
                    treeType = type->zType;
                }
                for(u32 x=0; x<assdecl->lhsCount; x++){
                    if(getVariableEntity(assdecl->lhs[x], scopes)){
                        lexer.emitErr(assdecl->tokenOff, "Invalid LHS");
                        return false;
                    };
                    VariableEntity *entity = createVariableEntity(assdecl->lhs[x], scopes[scopes.count-1]);
                    entity->pointerDepth = treePointerDepth;
                    entity->type = treeType;
                };
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
    return checkScope(lexer, file, scopes);
};