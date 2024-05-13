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

bool checkScope(Lexer &lexer, ASTFile &file, DynamicArray<Scope*> &scopes){
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