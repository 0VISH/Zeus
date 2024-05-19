#define BUCKET_BUFFER_SIZE 1024
#define REGS 25
#define START_FREE_REG 6

struct VarInfo{
    u32 off;    //offset from fp
    bool dw;    //dw or w?
};
struct Area{
    HashmapStr varToOff;          //maps var name to offset in offs
    DynamicArray<VarInfo> infos;
    
    void init(){
        varToOff.init();
        infos.init();
    }
    void uninit(){
        varToOff.uninit();
        infos.uninit();
    }
};
struct Register{
    s32 off;      //empty: -1
    u32 gen;      //generation of area
};
struct ASMBucket{
    char buffer[BUCKET_BUFFER_SIZE];
    ASMBucket *next;
};
struct ASMFile{
    DynamicArray<Area> areas;
    ASMBucket *start;
    ASMBucket *cur;
    /*
    NOTE: we do not follow RISC-V register convention
    new convention is
    x0 -> hard 0
    x1 -> ra(return address)
    x2 -> sp(stack pointer)
    x3 -> gp(global pointer)
    x4 -> tp(thread pointer)
    x5 -> fp(frame pointer)
    (x6,x31) -> 25 free regs
    */
    Register regs[REGS];
    u32 cursor;                //cursor for ASMBucket buffer

    void init(){
        cursor = 0;
        areas.init(10);
        start = (ASMBucket*)mem::alloc(sizeof(ASMBucket));
        start->next = nullptr;
        cur = start;
        memset(regs, -1, sizeof(Register)*25);
    };
    void uninit(){
        areas.uninit();
        while(start){
            ASMBucket *temp = start;
            start = start->next;
            mem::free(temp);
        };
    };
    void write(char *fmt, ...){
        va_list args;
        va_start(args, fmt);
        s32 res = vsnprintf(&cur->buffer[cursor], BUCKET_BUFFER_SIZE, fmt, args);
        va_end(args);
        if(res + cursor >= BUCKET_BUFFER_SIZE){
            cur->buffer[cursor] = '\0';
            ASMBucket *buc = (ASMBucket*)mem::alloc(sizeof(ASMBucket));
            buc->next = nullptr;
            cursor = 0;
            cur->next = buc;
            cur = buc;
            va_start(args, fmt);
            res = vsnprintf(cur->buffer, BUCKET_BUFFER_SIZE, fmt, args);
            va_end(args);
        };
        cursor += res;
    };
};

void store(u32 x, ASMFile &file){
    VarInfo info = file.areas[file.regs[x].gen].infos[x];
    file.write("%s x%d, %d(x5)", info.dw?"sd":"sw", x+START_FREE_REG, info.off);
};
void setRegister(u32 x, u32 gen, ASMFile &file){
    file.regs[x].gen = gen;
    file.regs[x].off = 0;   //setting to non -1 value. Maybe set to different values based on what resided?
};
u32 getOrCreateFreeRegister(ASMFile &file){
    u32 curGen = file.areas.count;
    for(u32 x=0; x<REGS; x++){
        if(file.regs[x].off == -1){return x;};
    };
    for(u32 x=0; x<REGS; x++){
        if(file.regs[x].gen < curGen){
            store(x, file);
            return x;
        };
    };
    store(0, file);
    return 0;
};
u32 lowerExpression(ASTBase *node, ASMFile &file){
    switch(node->type){
        case ASTType::INTEGER:{
            /*
            NOTE: we can scan registers to see if any one of them hold the requried
            constant integer, but since the chances are slim, we don't do it
            */
            ASTNum *num = (ASTNum*)node;
            u32 reg = getOrCreateFreeRegister(file);
            setRegister(reg, file.areas.count-1, file);
            file.write("li %lld\n", num->integer);
            return reg;
        }break;
    };
    return 0;
};
void lowerASTNode(ASTBase *node, ASMFile &file){
    switch(node->type){
        case ASTType::DECLERATION:{
            ASTAssDecl *assdecl = (ASTAssDecl*)node;
            u32 reg = lowerExpression(assdecl, file);

        }break;
    };
};
void lowerASTFile(ASTFile &file, FILE *outFile){
    ASMFile AsmFile;
    AsmFile.init();
    for(u32 x=0; x<file.nodes.count; x++){
        lowerASTNode(file.nodes[x], AsmFile);
    };
    //TODO: write to file
    AsmFile.uninit();
}