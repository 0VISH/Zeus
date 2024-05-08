#define AST_PAGE_SIZE 1024
#define BRING_TOKENS_TO_SCOPE DynamicArray<TokType> &tokTypes = lexer.tokenTypes;DynamicArray<TokenOffset> &tokOffs = lexer.tokenOffsets;

enum class ASTType{
    IDENTIFIER,
    DECLERATION,
    ASSIGNMENT,
    INTEGER,
    DECIMAL,
    TYPE,

    B_START,  //binary operators start
    B_ADD,
    B_SUB,
    B_MUL,
    B_DIV,
    B_MOD,
    B_END,    //binary operators end
};

struct ASTBase{
    ASTType type;
};
struct ASTIdentifier : ASTBase{
    String name;
};
struct ASTBinOp : ASTBase{
    ASTBase *lhs;
    ASTBase *rhs;
    u32 tokenOff;
    bool hasBracket;
};
struct ASTTypeNode : ASTBase{
    union{
        Type zType;
        u32 tokenOff;
    };
    u8 pointerDepth;
};
struct ASTAssDecl : ASTBase{
    String  *lhs;
    ASTBase *rhs;
    ASTTypeNode *zType;
    u32 lhsCount;
};
struct ASTNum : ASTBase{
    union{
        s64 integer;
        f64 decimal;
    };
};

struct ASTFile{
    DynamicArray<char*>    pages;
    DynamicArray<ASTBase*> nodes;
    u32 curPageWatermark;

    void init(){
        pages.init();
        nodes.init();
        pages.push((char*)mem::alloc(AST_PAGE_SIZE));
        curPageWatermark = 0;
    };
    void uninit(){
        for(u32 x=0; x<pages.count; x++) mem::free(pages[x]);
        pages.uninit();
        nodes.uninit();
    };
    ASTBase* newNode(u64 size, ASTType type){
        if(curPageWatermark+size >= AST_PAGE_SIZE){
            pages.push((char*)mem::alloc(AST_PAGE_SIZE));
            curPageWatermark = 0;
        };
        ASTBase *node = (ASTBase*)(pages[pages.count-1] + curPageWatermark);
        curPageWatermark += size;
        node->type = type;
        return node;
    };
    //bump-allocator for AST node members
    void* balloc(u64 size){
        if(curPageWatermark+size >= AST_PAGE_SIZE){
            pages.push((char*)mem::alloc(AST_PAGE_SIZE));
            curPageWatermark = 0;
        };
        char *mem = pages[pages.count-1] + curPageWatermark;
        curPageWatermark += size;
        return mem;
    };
};

//TODO: Hacked together af. REWRITE(return u64)
s64 string2int(const String &str){
    s64 num = 0;
    u32 len = str.len;
    for(u32 x=0; x<str.len; x+=1){
        char c = str[x];
        switch(c){
        case '.':
        case '_':
            len -= 1;
            continue;
        };
    };
    u32 y = 0;
    for(u32 x=0; x<str.len; x+=1){
        char c = str[x];
        switch(c){
        case '.':
        case '_':
            continue;
        };
        num += (c - '0') * pow(10, len-y-1);
        y += 1;
    };
    return num;
}
f64 string2float(const String &str){
    u32 decimal = 0;
    while(str[decimal] != '.'){decimal += 1;};
    u32 postDecimalBadChar = 0;
    for(u32 x=decimal+1; x<str.len; x+=1){
    	if(str[x] == '_'){postDecimalBadChar += 1;};
    };
    s64 num = string2int(str);
    return (f64)num/pow(10, str.len-decimal-1-postDecimalBadChar);
};
String makeStringFromTokOff(u32 x, Lexer &lexer){
    BRING_TOKENS_TO_SCOPE;
    TokenOffset off = tokOffs[x];
    String str;
    str.len = off.len;
    str.mem = lexer.fileContent + off.off;
    return str;
};
u32 getOperatorPriority(ASTType op){
    switch(op){
        case ASTType::B_ADD:
        case ASTType::B_SUB: return 1;
        case ASTType::B_MUL:
        case ASTType::B_DIV:
        case ASTType::B_MOD: return 2;
    };
    return 0;
};
ASTTypeNode* genASTTypeNode(Lexer &lexer, ASTFile &file, u32 &xArg){
    BRING_TOKENS_TO_SCOPE;
    u32 x = xArg;
    DEFER(xArg = x);
    u8 pointerDepth = 0;
    while(tokTypes[x] == (TokType)'^'){
        pointerDepth++;
        x++;
    };
    if(isType(tokTypes[x]) == false && tokTypes[x] != TokType::IDENTIFIER){
        lexer.emitErr(tokOffs[x].off, "Expected a type");
        return nullptr;
    };
    ASTTypeNode *type = (ASTTypeNode*)file.newNode(sizeof(ASTTypeNode), ASTType::TYPE);
    type->tokenOff = x++;
    type->pointerDepth = pointerDepth;
    return type;
};
ASTBase* _genASTExprTree(Lexer &lexer, ASTFile &file, u32 &xArg, u32 end, s8 &bracketArg){
    BRING_TOKENS_TO_SCOPE;
    u32 x = xArg;
    s8 bracket = bracketArg;
    bool hasBracket = false;
    DEFER({
        xArg = x;
        bracketArg = bracket;
    });
    //opening bracket '('
    if(tokTypes[x] == (TokType)'('){
        hasBracket=true;
        while(tokTypes[x] == (TokType)'('){
            x++;
            bracket++;
        };
    };
    //build operand
    ASTBase *lhs;
    switch(tokTypes[x]){
        case TokType::INTEGER:{
            s64 value = string2int(makeStringFromTokOff(x, lexer));
            ASTNum *num = (ASTNum*)file.newNode(sizeof(ASTNum), ASTType::INTEGER);
            num->integer = value;
            lhs = num;
        }break;
        case TokType::DECIMAL:{
            f64 value = string2float(makeStringFromTokOff(x, lexer));
            ASTNum *num = (ASTNum*)file.newNode(sizeof(ASTNum), ASTType::DECIMAL);
            num->decimal = value;
            lhs = num;
        }break;
        default:{
            lexer.emitErr(tokOffs[x].off, "Invalid operand");
            return nullptr;
        }break;
    };
    x++;
    if(x == end) return lhs;
    //closing bracket ')'
    if(tokTypes[x] == (TokType)')'){
        hasBracket=false;
        while(tokTypes[x] == (TokType)')'){
            x++;
            bracket--;
        }
    };
    if(x == end) return lhs;
    //build operator
    ASTType type;
    switch (tokTypes[x]) {
        case (TokType)'-': type = ASTType::B_SUB; break;
        case (TokType)'+': type = ASTType::B_ADD; break;
        case (TokType)'*': type = ASTType::B_MUL; break;
        case (TokType)'/': type = ASTType::B_DIV; break;
        default:{
            lexer.emitErr(tokOffs[x].off, "Invalid operator");
            return nullptr;
        }break;
    };
    ASTBinOp *binOp = (ASTBinOp*)file.newNode(sizeof(ASTBinOp), type);
    binOp->tokenOff = x;
    binOp->hasBracket = hasBracket;
    x++;
    //build rest of expression
    ASTBase *rhs = _genASTExprTree(lexer, file, x, end, bracket);
    if(!rhs){return nullptr;};
    binOp->lhs = lhs;
    binOp->rhs = rhs;
    if(rhs->type > ASTType::B_START && rhs->type < ASTType::B_END){
        u32 rhsPriority = getOperatorPriority(rhs->type);
        u32 curPriority = getOperatorPriority(binOp->type);
        ASTBinOp *rhsBin = (ASTBinOp*)rhs;
        if(rhsPriority < curPriority && !rhsBin->hasBracket){
            //fix tree branches(https://youtu.be/MnctEW1oL-E?si=6NnDgPSeX0F-aFD_&t=3696)
            binOp->rhs = rhsBin->lhs;
            rhsBin->lhs = binOp;
            return rhsBin;
        };
    };
    return binOp;
};
ASTBase* genASTExprTree(Lexer &lexer, ASTFile &file, u32 &x, u32 end){
    BRING_TOKENS_TO_SCOPE;
    s8 bracket = 0;
    u32 start = x;
    ASTBase *tree = _genASTExprTree(lexer, file, x, end, bracket);
    if(bracket < 0){
        lexer.emitErr(tokOffs[start].off, "Expected %d opening bracket%sin this expression", bracket*-1, (bracket==1)?" ":"s ");
        return nullptr;
    }else if(bracket > 0){
        lexer.emitErr(tokOffs[start].off, "Expected %d closing bracket%sin this expression", bracket, (bracket==1)?" ":"s ");
        return nullptr;
    };
    return tree;
};

inline bool isEndOfLineOrFile(DynamicArray<TokType> &types, u32 x){
    return types[x] == (TokType)'\n' || types[x] == TokType::END_OF_FILE;
};
inline u32 getEndOfLineOrFile(DynamicArray<TokType> &types, u32 x){
    while(!isEndOfLineOrFile(types, x)) x++;
    return x;
}
bool parseBlock(Lexer &lexer, ASTFile &file, u32 &xArg){
    BRING_TOKENS_TO_SCOPE;
    u32 x = xArg;
    DEFER(xArg = x);
    u32 start = x;
    switch(tokTypes[x]){
        case TokType::IDENTIFIER:
            x++;
            switch(tokTypes[x]){
                case (TokType)',':
                case (TokType)'=':
                case (TokType)':':{
                    //decleration (or) assignment
                    while(tokTypes[x] != (TokType)':' && tokTypes[x] != (TokType)'='){
                        if(tokTypes[x] != (TokType)','){
                            lexer.emitErr(tokOffs[x].off, "Expected ',' or ':'");
                            return false;
                        };
                        x++;
                        if(tokTypes[x] != TokType::IDENTIFIER){
                            lexer.emitErr(tokOffs[x].off, "Expected an identifier");
                            return false;
                        };
                        x++;
                    };
                    ASTAssDecl *assdecl = (ASTAssDecl*)file.newNode(sizeof(ASTAssDecl), ASTType::DECLERATION);
                    u32 lhsCount = ((x-start)/2) + 1;
                    String *strs = (String*)file.balloc(lhsCount * sizeof(String));
                    assdecl->lhsCount = lhsCount;
                    assdecl->lhs = strs;
                    for(u32 i=start, j=0; i<x; i+=2, j++){
                        String &str = strs[j];
                        const TokenOffset &off = tokOffs[i];
                        str.len = off.len;
                        str.mem = lexer.fileContent + off.off;
                    };
                    if(tokTypes[x] == (TokType)'='){assdecl->type = ASTType::ASSIGNMENT;}
                    else{
                        x++;
                        if(tokTypes[x] != (TokType)'='){
                            ASTTypeNode *type = genASTTypeNode(lexer, file, x);
                            assdecl->zType = type;
                            if(tokTypes[x] != (TokType)'='){
                                lexer.emitErr(tokOffs[x].off, "Expected '='");
                                return false;
                            };
                        }else{assdecl->zType = nullptr;};
                    };
                    x++;
                    ASTBase *expr = genASTExprTree(lexer, file, x, getEndOfLineOrFile(lexer.tokenTypes, x));
                    if(!expr) return false;
                    assdecl->rhs = expr;
                    file.nodes.push(assdecl);
                }break;
            };
    };
    return true;
};

#if(DBG)

#define PLOG(fmt, ...) pad(padding);printf(fmt, __VA_ARGS__)

namespace dbg{
    inline void pad(u8 padding){
        printf("\n");
        for(u8 x=0; x<padding; x++) printf("    ");
    };
    inline void dumpStrings(String *strs, u32 count){
        for(u32 x=0; x<count; x++){
            const String &str = strs[x];
            printf("%.*s ", str.len, str.mem);
        };
    };
    void dumpASTNode(ASTBase *node, Lexer &lexer, u8 padding){
        PLOG("[NODE]");
        PLOG("type: ");
        bool hasNotDumped = true;
        switch(node->type){
            case ASTType::ASSIGNMENT:{
                ASTAssDecl *assdecl = (ASTAssDecl*)node;
                printf("assignment");
                PLOG("lhs: ");
                dumpStrings(assdecl->lhs, assdecl->lhsCount);
                PLOG("rhs:");
                dumpASTNode(assdecl->rhs, lexer, padding+1);
            }break;
            case ASTType::DECLERATION:{
                ASTAssDecl *assdecl = (ASTAssDecl*)node;
                printf("decleration");
                if(assdecl->zType){
                    PLOG("type:");
                    dumpASTNode(assdecl->zType, lexer, padding+1);
                };
                PLOG("lhs: ");
                dumpStrings(assdecl->lhs, assdecl->lhsCount);
                if(assdecl->rhs){
                    PLOG("rhs:");
                    dumpASTNode(assdecl->rhs, lexer, padding+1);
                };
            }break;
            case ASTType::INTEGER:{
                ASTNum *num = (ASTNum*)node;
                printf("integer");
                PLOG("value: %lld", num->integer);
            }break;
            case ASTType::TYPE:{
                ASTTypeNode *type = (ASTTypeNode*)node;
                printf("type");
                String ztype = makeStringFromTokOff(type->tokenOff, lexer);
                PLOG("z_type: %.*s", ztype.len, ztype.mem);
            }break;
            case ASTType::B_ADD: if(hasNotDumped){printf("add");hasNotDumped=false;};
            case ASTType::B_SUB: if(hasNotDumped){printf("sub");hasNotDumped=false;};
            case ASTType::B_MUL: if(hasNotDumped){printf("mul");hasNotDumped=false;};
            case ASTType::B_DIV:{
                if(hasNotDumped){printf("div");hasNotDumped=false;};
                ASTBinOp *op = (ASTBinOp*)node;
                PLOG("lhs:");
                dumpASTNode(op->lhs, lexer, padding+1);
                PLOG("rhs:");
                dumpASTNode(op->rhs, lexer, padding+1);
            }break;
            default: UNREACHABLE;
        };
    };
    void dumpASTFile(ASTFile &file, Lexer &lexer){
        for(u32 x=0; x<file.nodes.count; x++){
            dumpASTNode(file.nodes[x], lexer, 0);
        };
        printf("\n");
    };
};
#endif