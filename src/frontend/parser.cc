#define AST_PAGE_SIZE 1024
#define BRING_TOKENS_TO_SCOPE DynamicArray<TokType> &tokTypes = lexer.tokenTypes;DynamicArray<TokenOffset> &tokOffs = lexer.tokenOffsets;

enum class ASTType{
    IDENTIFIER,
    DECLERATION,
    ASSIGNMENT,

    B_ADD,   //binary operators start
    B_SUB,
    B_MUL,
    B_DIV,
    B_MOD,   //binary operators stop
};

struct ASTBase{
    ASTType type;
};
struct ASTIdentifier : ASTBase{
    String name;
};
struct ASTBin : ASTBase{
    ASTBase *lhs;
    ASTBase *rhs;
};
struct ASTAssDecl : ASTBase{
    String  *lhs;
    ASTBase *rhs;
    u32 lhsCount;
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

inline bool isEndOfLineOrFile(DynamicArray<TokType> &types, u32 x){
    return types[x] == (TokType)'\n' || types[x] == TokType::END_OF_FILE;
};
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
                    ASTType type = ASTType::DECLERATION;
                    if(tokTypes[x] == (TokType)'=') type = ASTType::ASSIGNMENT;
                    ASTAssDecl *assdecl = (ASTAssDecl*)file.newNode(sizeof(ASTAssDecl), type);
                    u32 lhsCount = ((x-start)/2) + 1;
                    String *strs = (String*)file.balloc(lhsCount * sizeof(String));
                    assdecl->lhsCount = lhsCount;
                    assdecl->lhs = strs;
                    for(u32 i=start; i<x; i+=2){
                        String &str = strs[i];
                        const TokenOffset &off = tokOffs[i];
                        str.len = off.len;
                        str.mem = lexer.fileContent + off.off;
                    };
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
    void dumpASTNode(ASTBase *node, u8 padding=0){
        PLOG("[NODE]");
        PLOG("type: ");
        switch(node->type){
            case ASTType::ASSIGNMENT:{
                ASTAssDecl *assdecl = (ASTAssDecl*)node;
                printf("assignment");
                PLOG("lhs: ");
                dumpStrings(assdecl->lhs, assdecl->lhsCount);
            }break;
            case ASTType::DECLERATION:{
                ASTAssDecl *assdecl = (ASTAssDecl*)node;
                printf("decleration");
                PLOG("lhs: ");
                dumpStrings(assdecl->lhs, assdecl->lhsCount);
            }break;
            default: UNREACHABLE;
        };
    };
    void dumpASTFile(ASTFile &file){
        for(u32 x=0; x<file.nodes.count; x++){
            dumpASTNode(file.nodes[x]);
        };
    };
};
#endif