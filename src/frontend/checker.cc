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

static Scope *globalScopes;                //all file scopes
static Scope *scopeAllocMem;               //all scopes
static u32 scopeOff;                       //how many total scopes
static HashmapStr struc;                   //all structs name to off
static DynamicArray<StructEntity> strucs;  //all structs

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
StructEntity *getStructEntity(String name){
    u32 off;
    if(!struc.getValue(name, &off)) return nullptr;
    return &strucs[off];
};
StructEntity *getStructEntity(Type type){
    if((u32)type > strucs.count) return nullptr;
    return &strucs[(u32)type];
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
        node->zType = (Type)((u32)tokTypes[node->tokenOff] - (u32)TokType::K_TYPE_START + 1);  //+1 since Type::BOOL
        return true;
    };
    if(tokTypes[node->tokenOff] != TokType::IDENTIFIER){
        lexer.emitErr(tokOffs[node->tokenOff].off, "Expected a type or a structure name");
        return false;
    };
    String name = makeStringFromTokOff(node->tokenOff, lexer);
    u32 off;
    if(!struc.getValue(name, &off)){
        lexer.emitErr(tokOffs[node->tokenOff].off, "Structure not defined");
        return false;
    };
    node->zType = (Type)off;
    return true;
};
Type checkModifierChain(Lexer &lexer, ASTBase *root, VariableEntity *entity){
    BRING_TOKENS_TO_SCOPE;
    Type structType = entity->type;
    StructEntity *structEntity = getStructEntity(structType);
    Scope *structBodyScope = structEntity->body;
    while(root){
        switch(root->type){
            case ASTType::MODIFIER:{
                ASTModifier *mod = (ASTModifier*)root;
                u32 off;
                if(!structBodyScope->var.getValue(mod->name, &off)){
                    lexer.emitErr(tokOffs[mod->tokenOff].off, "%.*s does not belong to the defined structure", mod->name.len, mod->name.mem);
                    return Type::INVALID;
                }
                return checkModifierChain(lexer, mod->child, &structBodyScope->vars[off]);
            }break;
            //TODO: array_at
            case ASTType::VARIABLE:{
                ASTVariable *var = (ASTVariable*)root;
                u32 off;
                if(!structBodyScope->var.getValue(var->name, &off)){
                    lexer.emitErr(tokOffs[var->tokenOff].off, "%.*s does not belong to the defined structure", var->name.len, var->name.mem);
                    return Type::INVALID;
                };
                return structBodyScope->vars[off].type;
            }break;
        };
    };
    return Type::INVALID;
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
            if(entity == nullptr){
                ASTVariable *var = (ASTVariable*)node;
                lexer.emitErr(tokOffs[var->tokenOff].off, "Variable not defined");
                return Type::INVALID;
            };
            pointerDepth = entity->pointerDepth;
            return entity->type;
        }break;
        case ASTType::MODIFIER:{
            ASTModifier *mod = (ASTModifier*)node;
            VariableEntity *entity = getVariableEntity(node, scopes);
            if(entity == nullptr){
                lexer.emitErr(tokOffs[mod->tokenOff].off, "Variable not defined");
                return Type::INVALID;
            };
            return checkModifierChain(lexer, mod->child, entity);
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
    u64 size;
    switch(typeType){
        case Type::S64:
        case Type::U64:  size = 64;break;
        case Type::BOOL:
        case Type::S32:
        case Type::U32:  size = 32;break;
        case Type::S16:
        case Type::U16:  size = 16;break;
        case Type::CHAR:
        case Type::S8:
        case Type::U8:   size = 8;break;
        default:{
            ASSERT(assdecl->zType);
            StructEntity *structEntity = getStructEntity(typeType);
            size = structEntity->size;
        }break;
    };
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
                if(getStructEntity(Struct->name)){
                    lexer.emitErr(tokOffs[Struct->tokenOff].off, "Structure already defined");
                    return false;
                };
                u32 id = strucs.count;
                struc.insertValue(Struct->name, id);
                StructEntity *entity = &strucs.newElem();
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
                if(checkAss(lexer, (ASTAssDecl*)node, scopes) == 0) return false;
            }break;
            case ASTType::ASSIGNMENT:{
                ASTAssDecl *assdecl = (ASTAssDecl*)node;
                u32 treePointerDepth;
                Type treeType = checkTree(lexer, assdecl->rhs, scopes, treePointerDepth);
                if(treeType == Type::INVALID) return false;
                for(u32 x=0; x<assdecl->lhsCount; x++){
                    ASTBase *node = assdecl->lhs[x];
                    VariableEntity *entity = getVariableEntity(node, scopes);
                    if(entity == nullptr){
                        if(node->type == ASTType::VARIABLE || node->type == ASTType::MODIFIER){
                                lexer.emitErr(tokOffs[assdecl->tokenOff].off, "Variable not defined in LHS(%d)", x);
                                return false;
                        };
                        lexer.emitErr(tokOffs[assdecl->tokenOff].off, "Only variable or modifiers allowed in LHS");
                        return false;
                    };
                    if(node->type == ASTType::MODIFIER){
                        ASTModifier *mod = (ASTModifier*)node;
                        if(checkModifierChain(lexer, mod->child, entity) == Type::INVALID) return false;
                    };
                };
            }break;
            case ASTType::IF:{
                ASTIf *If = (ASTIf*)node;
                u32 treePointerDepth;
                Type treeType = checkTree(lexer, If->expr, scopes, treePointerDepth);
                if(treeType == Type::INVALID) return false;
                if(treeType > Type::COUNT && treePointerDepth == 0){
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