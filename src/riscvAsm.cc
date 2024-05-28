#define BUCKET_BUFFER_SIZE 1024
#define REGS 25
#define START_FREE_REG 6
#define PROC_ARG_REG_COUNT 5
#define FREE_REG       -1
#define CONST_IN_REG   -2
#define GLOBAL_IN_REG  -3
#define INVALID_REG    40

#if(WIN)
#define WRITE(file, buff, len) WriteFile(file, buff, len, nullptr, NULL)
#elif(LIN)
#define WRITE(file, buff, len) write(file, buff, len)
#endif

struct VarInfo{
    s32 fpOff;    //offset from fp
    bool dw;      //dw or w?
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
struct Register : VarInfo{
    String globalName;
    u32 gen;
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
    u32 fpOff;                 //off from fp
    u32 cursor;                //cursor for ASMBucket buffer

    void init(){
        fpOff = 0;
        cursor = 0;
        areas.init(10);
        Area &fileArea = areas.newElem();
        fileArea.init();
        start = (ASMBucket*)mem::alloc(sizeof(ASMBucket));
        start->buff[BUCKET_BUFFER_SIZE] = '\0';
        start->next = nullptr;
        cur = start;
        memset(regs, FREE_REG, sizeof(Register)*REGS);
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

static HashmapStr globalToOff;
static DynamicArray<VarInfo> globalInfos;

inline void store(u32 reg, ASMFile &file){
    Register regi = file.regs[reg];
    if(regi.fpOff == FREE_REG || regi.fpOff == CONST_IN_REG){
        file.regs[reg].fpOff = FREE_REG;
        return;
    };
    if(regi.fpOff == GLOBAL_IN_REG){
        u32 memReg = INVALID_REG;
        for(u32 x=0; x<REGS; x++){
            if(file.regs[x].fpOff == FREE_REG && file.regs[x].fpOff == CONST_IN_REG){
                memReg = x;
                break;
            };    
        };
        if(memReg == INVALID_REG){
            memReg = reg + 1;
            if(reg == REGS) memReg = reg - 1;
            store(memReg, file);
        };
        file.write("la x%d, %.*s\n%s x%d, 0(x%d)", memReg+START_FREE_REG, regi.globalName.len, regi.globalName.mem, (regi.dw)?"sd":"sw", reg+START_FREE_REG, memReg+START_FREE_REG);
        file.regs[memReg].fpOff = FREE_REG;
    }else file.write("%s x%d, %d(x5)", regi.dw?"sd":"sw", reg+START_FREE_REG, regi.fpOff);
    file.regs[reg].fpOff = FREE_REG;
};
inline void setRegister(u32 reg, u32 gen, String globalName, VarInfo info, ASMFile &file){
    Register &regist = file.regs[reg];
    regist.gen = gen;
    regist.fpOff = info.fpOff;
    regist.dw = info.dw;
    regist.globalName = globalName;
};
u32 getOrCreateFreeRegister(ASMFile &file){
    u32 curGen = file.areas.count;
    for(u32 x=0; x<REGS; x++){
        if(file.regs[x].fpOff == FREE_REG) return x;
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
VarInfo getVarInfo(String &name, ASMFile &file, u32 *generation = nullptr){
    u32 curGen = file.areas.count;
    u32 gen = curGen;
    while(gen>0){
        u32 off;
        if(file.areas[--gen].varToOff.getValue(name, &off)){
            if(generation) *generation = gen;
            return file.areas[gen].infos[off];
        };
    };
    if(generation) *generation = 0;
    u32 off;
    globalToOff.getValue(name, &off);
    return globalInfos[off];
};
u32 getOrLoadToRegister(String &name, ASMFile &file, bool loadOnlyAddress = false){
    u32 gen;
    VarInfo info = getVarInfo(name, file, &gen);
    for(u32 x=0; x<REGS; x++){
        if(file.regs[x].gen == gen && file.regs[x].fpOff == info.fpOff) return x;
    };
    u32 freeReg = getOrCreateFreeRegister(file);
    if(info.fpOff != GLOBAL_IN_REG){
        file.write("%s x%d, %d(x5)", info.dw?"ld":"lw", freeReg+START_FREE_REG, info.fpOff);
        setRegister(freeReg, gen, name, info, file);
        return freeReg;
    };
    file.write("la x%d, %.*s\n%s x%d, 0(x%d)", freeReg+START_FREE_REG, name.len, name.mem, info.dw?"ld":"lw", freeReg+START_FREE_REG, freeReg+START_FREE_REG);
    setRegister(freeReg, gen, name, info, file);
    return freeReg;
};
u32 lowerExpression(ASTBase *node, ASMFile &file){
    switch(node->type){
        case ASTType::U_MEM:{
            node = ((ASTUnOp*)node)->child;
            switch(node->type){
                case ASTType::VARIABLE:{
                    ASTVariable *var = (ASTVariable*)node;
                    VarInfo info = getVarInfo(var->name, file);
                    u32 reg = getOrCreateFreeRegister(file);
                    file.write("addi x%d, x5, %d", reg+START_FREE_REG, info.fpOff);
                    return reg;
                }break;
                default: UNREACHABLE;
            };
        }break;
        case ASTType::INTEGER:{
            /*
            NOTE: we can scan registers to see if any one of them hold the requried
            constant integer, but since the chances are slim, we don't do it
            */
            ASTNum *num = (ASTNum*)node;
            u32 reg = getOrCreateFreeRegister(file);
            bool dw = false;
            if(num->integer > 2147483647) dw = true;
            setRegister(reg, file.areas.count-1, {nullptr, 0}, {CONST_IN_REG, dw}, file);
            file.write("li x%d, %lld", reg+START_FREE_REG, num->integer);
            return reg;
        }break;
        case ASTType::VARIABLE:{
            ASTVariable *var = (ASTVariable*)node;
            return getOrLoadToRegister(var->name, file);
        }break;
        default: UNREACHABLE;
    };
    return 0;
};

static u32 labelId = 0;

void lowerASTNode(ASTBase *node, ASMFile &file){
    switch(node->type){
        case ASTType::FOR:{
            ASTFor *For = (ASTFor*)node;
            u32 labelBegin = labelId++;
            if(For->expr == nullptr && For->initializer == nullptr){
                //for-ever
                file.write(".L%d:", labelBegin);
                for(u32 x=0; x<For->bodyCount; x++){
                    lowerASTNode(For->body[x], file);
                };
                file.write("j .L%d", labelBegin);
            }else if(For->initializer != nullptr){
                //c-for
            }else{
                //c-while
                u32 labelEnd = labelId++;
                u32 reg = lowerExpression(For->expr, file);
                file.write(".L%d:\nbeq x%d, zero, .L%d", labelBegin, reg+START_FREE_REG, labelEnd);
                for(u32 x=0; x<For->bodyCount; x++){
                    lowerASTNode(For->body[x], file);
                };
                file.write("j .L%d\n.L%d:", labelBegin, labelEnd);
            };
        }break;
        case ASTType::IF:{
            ASTIf *If = (ASTIf*)node;
            u32 endLabel = labelId++;
            u32 reg = lowerExpression(If->expr, file);
            file.write("beq x%d, zero, .L%d", reg+START_FREE_REG, endLabel);
            for(u32 x=0; x<If->ifBodyCount; x++){
                lowerASTNode(If->ifBody[x], file);
            };
            if(If->elseBodyCount > 0){
                u32 newEndLabel = labelId++;
                file.write("j .L%d\n.L%d:", newEndLabel, endLabel);
                for(u32 x=0; x<If->elseBodyCount; x++){
                    lowerASTNode(If->elseBody[x], file);
                };
                endLabel = newEndLabel;
            }
            file.write(".L%d:", endLabel);
        }break;
        case ASTType::PROC_DEF:{
            ASTProcDefDecl *proc = (ASTProcDefDecl*)node;
            file.write("%.*s:", proc->name.len, proc->name.mem);
            u32 stackSize = 0;
            if(cmpString(proc->name, "main")){
                stackSize = (u32)(pStackSize * 1000);
                file.write("li x6, %d\nsub sp, sp, x6\naddi x5, sp, 0", stackSize);
            };
            Area &procArea = file.areas.newElem();
            procArea.init();
            u32 procArgRegisterCount = 0;
            u32 stackAbove = 0;
            u32 stackBelow = 0;
            for(u32 x=proc->inputCount; x>0;){
                ASTAssDecl *decl = proc->inputs[--x];
                for(u32 i=0; i<decl->lhsCount; i++){
                    ASTVariable *var = (ASTVariable*)decl->lhs[i];
                    procArea.varToOff.insertValue(var->name, procArea.infos.count);
                    VarInfo &info = procArea.infos.newElem();
                    if(var->entity->size > 64 || procArgRegisterCount >= PROC_ARG_REG_COUNT){
                        stackBelow += var->entity->size;
                        info.fpOff = -1 * (stackBelow);
                        info.dw  = var->entity->size > 64;
                    }else{
                        stackAbove += var->entity->size;
                        info.fpOff = stackAbove;
                        info.dw = false;
                        setRegister(procArgRegisterCount, file.areas.count-1, {nullptr, 0}, {info.fpOff, false}, file);
                        procArgRegisterCount++;
                    };
                };
            };
            for(u32 x=0; x<proc->bodyCount; x++) lowerASTNode(proc->body[x], file);
            for(u32 x=0; x<REGS; x++) store(x, file);
            file.areas.pop();
            if(stackSize != 0) file.write("li x6, %d\nadd sp, sp, x6", stackSize);
        }break;
        case ASTType::DECLERATION:{
            ASTAssDecl *decl = (ASTAssDecl*)node;
            if(decl->lhsCount > 1){
                //TODO:
            }else{
                u32 reg;
                if(decl->rhs) reg = lowerExpression(decl->rhs, file);
                else{
                    reg = getOrCreateFreeRegister(file);
                    file.write("addi x%d, x0, x0", reg+START_FREE_REG);
                };
                Area &curArea = file.areas[file.areas.count-1];
                ASTVariable *var = (ASTVariable*)decl->lhs[0];
                curArea.varToOff.insertValue(var->name, curArea.infos.count);
                VarInfo &info = curArea.infos.newElem();
                info.fpOff = file.fpOff;
                info.dw = (var->entity->size > 32 || var->entity->pointerDepth > 0) ? true:false;
                file.fpOff += (var->entity->pointerDepth > 0) ? 64:var->entity->size;
                Register &regi = file.regs[reg];
                regi.fpOff = info.fpOff;
                regi.dw = info.dw;
                regi.gen = file.areas.count-1;
            }
        }break;
        case ASTType::ASSIGNMENT:{
            ASTAssDecl *ass = (ASTAssDecl*)node;
            if(ass->lhsCount > 1){
                //TODO:
            }else{
                ASTVariable *var = (ASTVariable*)ass->lhs[0];
                u32 rhs = lowerExpression(ass->rhs, file);
                u32 lhs = getOrLoadToRegister(var->name, file);
                file.write("add x%d, x0, x%d", lhs+START_FREE_REG, rhs+START_FREE_REG);
            };
        }break;
    };
};
void lowerToRISCV(char *outputPath, DynamicArray<ASTBase*> &globals){
#if(WIN)
    HANDLE file = CreateFile(outputPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DEFER(CloseHandle(file));
#elif(LIN)
    int file = open(outputPath, O_RDWR|O_CREAT);
    DEFER(close(file));
#endif
    const u32 BUFF_SIZE = 1024;
    char buff[BUFF_SIZE];
    char *start = ".section .data\n";
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
    globalToOff.init();
    globalInfos.init();
    for(u32 x=0; x<globals.count; x++){
        ASTAssDecl *assdecl = (ASTAssDecl*)globals[x];
        ASTVariable *var = (ASTVariable*)assdecl->lhs[0];
        int temp;
        globalToOff.insertValue(var->name, globalInfos.count);
        VarInfo &info = globalInfos.newElem();
        info.fpOff = GLOBAL_IN_REG;
GLOBAL_WRITE_ASM_TO_BUFF:
        switch(assdecl->rhs->type){
            case ASTType::STRING:{
                ASTString *str = (ASTString*)assdecl->rhs;
                u32 off;
                stringToId.getValue(str->str, &off);
                temp = snprintf(buff+cursor, BUFF_SIZE-cursor, "%.*s: .dword _S%d\n", var->name.len, var->name.mem, off);
                info.dw = true;
            }break;
            case ASTType::CHARACTER:{
                ASTNum *num = (ASTNum*)assdecl->rhs;
                temp = snprintf(buff+cursor, BUFF_SIZE-cursor, ".%.*s: .byte %d\n", var->name.len, var->name.mem, (u8)num->integer);
                info.dw = false;
            }break;
            case ASTType::INTEGER:{
                ASTNum *num = (ASTNum*)assdecl->rhs;
                bool dw = false;
                if(var->entity->size > 32 || var->entity->pointerDepth > 0) dw = true;
                temp = snprintf(buff+cursor, BUFF_SIZE-cursor, ".%.*s: .%s %lld\n", var->name.len, var->name.mem, dw?"dword":"word", num->integer);
                info.dw = dw;
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
    char *textSection = "\n.section .text\n";
    WRITE(file, textSection, strlen(textSection));
    for(u32 x=linearDepEntities.count; x > 0;){
        x -= 1;
        FileEntity &fe = linearDepEntities[x];
        if(fe.file.nodes.count == 0) continue;
        ASTFile &astFile = fe.file;
        ASMFile AsmFile;
        AsmFile.init();
        for(u32 x=0; x<astFile.nodes.count; x++) lowerASTNode(astFile.nodes[x], AsmFile);
        ASMBucket *buc = AsmFile.start;
        AsmFile.uninit();
        while(buc){
            WRITE(file, buc->buff, strlen(buc->buff));
            ASMBucket *temp = buc;
            buc = buc->next;
            mem::free(temp);
        };
    };
};
