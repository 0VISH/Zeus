#define BUCKET_BUFFER_SIZE 1024
#define REGS 25
#define START_FREE_REG 6
#define PROC_ARG_REG   5

#if(WIN)
#define WRITE(file, buff, len) WriteFile(file, buff, len, nullptr, NULL)
#elif(LIN)
#define WRITE(file, buff, len) write(file, buff, len)
#endif

struct VarInfo{
    s32 off;    //offset from fp
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
    s64 off;      //empty: -1
    u8  gen;      //generation of area
};
struct ASMBucket{
    char buff[BUCKET_BUFFER_SIZE+1];    //+1 for null byte
    ASMBucket *next;
};
struct ASMFile{
    DynamicArray<Area> areas;
    ASMBucket *start;
    ASMBucket *cur;
    /*
    NOTE: we do not follow RISC-V register convention
    ----------FOLLOWING OLD CONVENTION----------
    x0 -> hard 0
    x1 -> ra(return address)
    x2 -> sp(stack pointer)
    x3 -> gp(global pointer)
    x4 -> tp(thread pointer)
    ----------NEW CONVENTION STARTS HERE----------
    x5 -> fp(frame pointer)
    (x6,x31) -> 25 free regs
    */
    Register regs[REGS];
    u32 off;                   //off from fp
    u32 cursor;                //cursor for ASMBucket buffer

    void init(){
        off = 0;
        cursor = 0;
        areas.init(10);
        Area &fileArea = areas.newElem();
        fileArea.init();
        start = (ASMBucket*)mem::alloc(sizeof(ASMBucket));
        start->buff[BUCKET_BUFFER_SIZE] = '\0';
        start->next = nullptr;
        cur = start;
        memset(regs, -1, sizeof(Register)*25);
    };
    void uninit(){
        for(u32 x=0; x<areas.count; x++) areas[x].uninit();
        areas.uninit();
    };
    void write(char *fmt, ...){
        va_list args;
        va_start(args, fmt);
        s32 res = vsnprintf(&cur->buff[cursor], BUCKET_BUFFER_SIZE, fmt, args);
        va_end(args);
        if(res + cursor + 1 >= BUCKET_BUFFER_SIZE){
            cur->buff[cursor] = '\0';
            ASMBucket *buc = (ASMBucket*)mem::alloc(sizeof(ASMBucket));
            buc->buff[BUCKET_BUFFER_SIZE] = '\0';
            buc->next = nullptr;
            cursor = 0;
            cur->next = buc;
            cur = buc;
            va_start(args, fmt);
            res = vsnprintf(cur->buff, BUCKET_BUFFER_SIZE, fmt, args);
            va_end(args);
        };
        cursor += res;
        cur->buff[cursor++] = '\n';
        cur->buff[cursor] = '\0';
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
            file.write("li x%d, %lld", reg+START_FREE_REG, num->integer);
            return reg;
        }break;
        default: UNREACHABLE;
    };
    return 0;
};
void lowerASTNode(ASTBase *node, ASMFile &file){
    switch(node->type){
        case ASTType::PROC_DEF:{
            ASTProcDefDecl *proc = (ASTProcDefDecl*)node;
            file.write("%.*s:", proc->name.len, proc->name.mem);
            u32 stackSize = 0;
            if(cmpString(proc->name, "main")){
                stackSize = (u32)(pStackSize * 1000);
                file.write("li x6, %d\nsub sp, sp, x6", stackSize);
            };
            Area &procArea = file.areas.newElem();
            procArea.init();
            u32 procArgRegisterCount = 0;
            u64 procArgTotalSize = 0;
            for(u32 x=proc->inputCount; x>0;){
                ASTAssDecl *decl = proc->inputs[--x];
                for(u32 i=0; i<decl->lhsCount; i++){
                    ASTVariable *var = (ASTVariable*)decl->lhs[i];
                    procArea.varToOff.insertValue(var->name, procArea.infos.count);
                    VarInfo &info = procArea.infos.newElem();
                    if(var->entity->size > 64){
                        info.off = -1 * (procArgTotalSize+var->entity->size);
                        info.dw  = true;
                    }else{
                        info.off = procArgTotalSize+var->entity->size;
                        info.dw = false;
                    };
                };
            };
            for(u32 x=0; x<proc->bodyCount; x++){
                lowerASTNode(proc->body[x], file);
            };
            file.areas.pop();
            if(stackSize != 0){
                file.write("li x6, %d\nadd sp, sp, x6", stackSize);
            };
        }break;
        case ASTType::DECLERATION:{
            ASTAssDecl *assdecl = (ASTAssDecl*)node;
            if(assdecl->lhsCount > 1){
                //TODO:
            }else{
                u32 reg = lowerExpression(assdecl->rhs, file);
                Area &curArea = file.areas[file.areas.count-1];
                ASTVariable *var = (ASTVariable*)assdecl->lhs[0];
                curArea.varToOff.insertValue(var->name, curArea.infos.count-1);
                VarInfo &info = curArea.infos.newElem();
                info.off = file.off;
                info.dw = (var->entity->size > 32 || var->entity->pointerDepth > 0) ? true:false;
                file.off += (var->entity->pointerDepth > 0) ? 64:var->entity->size;
            }
        }break;
    };
};
ASMBucket* lowerASTFile(ASTFile &file){
    ASMFile AsmFile;
    AsmFile.init();
    for(u32 x=0; x<file.nodes.count; x++){
        ASTBase *node = file.nodes[x];
        switch(node->type){
            case ASTType::ASSIGNMENT: break;
            default: lowerASTNode(node, AsmFile);
        };
    };
    ASMBucket *start = AsmFile.start;
    AsmFile.uninit();
    return start;
};

void lowerToRISCV(char *outputPath, DynamicArray<ASTBase*> &globals){
    //no buffering please
#if(WIN)
    HANDLE file = CreateFile(outputPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DEFER(CloseHandle(file));
#elif(LIN)
    int file = open(outputPath, O_RDWR|O_CREAT);
    DEFER(close(file));
#endif
    const u32 BUFF_SIZE = 1024;
    char buff[BUFF_SIZE];
    char *start = ".section data\n";
    u32 cursor = snprintf(buff, BUFF_SIZE, "%s", start);
    for(u32 x=0,i=0; x<stringToId.count;){
        if(stringToId.status[i]){
            DUMP_STRINGS:
            const String str = stringToId.keys[i];
            u32 temp = snprintf(buff+cursor, BUFF_SIZE-cursor, "_L%d: .ascii \"%.*s\"\n", x, str.len, str.mem);
            if(temp+cursor > BUFF_SIZE){
                WRITE(file, buff, cursor);
                cursor = 0;
                goto DUMP_STRINGS;
            };
            cursor += temp;
            x++;
        };
        i++;
    };
    for(u32 x=0; x<globals.count; x++){
        //TODO: strings
        ASTAssDecl *assdecl = (ASTAssDecl*)globals[x];
        ASTVariable *var = (ASTVariable*)assdecl->lhs[0];
        int temp;
GLOBAL_WRITE_ASM_TO_BUFF:
        switch(assdecl->rhs->type){
            case ASTType::STRING:{
                ASTString *str = (ASTString*)assdecl->rhs;
                u32 off;
                stringToId.getValue(str->str, &off);
                temp = snprintf(buff+cursor, BUFF_SIZE-cursor, "%.*s: .dword _S%d\n", var->name.len, var->name.mem, off);
            }break;
            case ASTType::CHARACTER:{
                ASTNum *num = (ASTNum*)assdecl->rhs;
                temp = snprintf(buff+cursor, BUFF_SIZE-cursor, ".%.*s: .byte %d\n", var->name.len, var->name.mem, (u8)num->integer);
            }break;
            case ASTType::INTEGER:{
                ASTNum *num = (ASTNum*)assdecl->rhs;
                bool dw = false;
                if(var->entity->size > 32 || var->entity->pointerDepth > 0) dw = true;
                temp = snprintf(buff+cursor, BUFF_SIZE-cursor, ".%.*s: .%s %lld\n", var->name.len, var->name.mem, dw?"dword":"word", num->integer);
            }break;
        };
        if(temp + cursor >= BUFF_SIZE){
            WRITE(file, buff, cursor);
            cursor = 0;
            goto GLOBAL_WRITE_ASM_TO_BUFF;
        };
        cursor += temp;
    };
    if(cursor) WRITE(file, buff, cursor);
    char *textSection = "\n.section text\n";
    WRITE(file, textSection, strlen(textSection));
    for(u32 x=linearDepEntities.count; x > 0;){
        x -= 1;
        FileEntity &fe = linearDepEntities[x];
        if(fe.file.nodes.count == 0) continue;
        ASMBucket *buc = lowerASTFile(fe.file);
        while(buc){
            WRITE(file, buc->buff, strlen(buc->buff));
            mem::free(buc);
            buc = buc->next;
        };
    };
};
