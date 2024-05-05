enum class TokType {
    END_OF_FILE,

    //room for ASCII chars

    IDENTIFIER = 256,
    INTEGER,
    DECIMAL,
    SINGLE_QUOTES,
    DOUBLE_QUOTES,
    DDOT,
    TDOT,

    K_START,    //keywords start
    K_U8,
    K_F32,
    K_F64,
    K_U16,
    K_U64,
    K_S64,
    K_U32,
    K_S8,
    K_S16,
    K_S32,
    K_PROC,
    K_IF,
    K_STRUCT,
    K_FOR,
    K_CONSTANT,
    K_RETURN,
    K_ELSE,
    K_END,       //keywords end

    P_START,     //poundwords start
    P_IMPORT,
    P_END,       //poundwords end
};
struct TokenOffset {
    u32 off;
    u16 len;
};

namespace Word{
    struct WordData {
        const char *str;
        const TokType type;
    };
    WordData keywordsData[] = {
        {"u8", TokType::K_U8},
        {"f32", TokType::K_F32},
        {"f64", TokType::K_F64},
        {"u16", TokType::K_U16},
        {"u32", TokType::K_U32},
        {"u64", TokType::K_U64},
        {"s64", TokType::K_S64},
        {"s8", TokType::K_S8},
        {"s16", TokType::K_S16},
        {"s32", TokType::K_S32 },
        {"proc", TokType::K_PROC},
        {"if", TokType::K_IF},
        {"struct", TokType::K_STRUCT},
        {"for", TokType::K_FOR},
        {"const", TokType::K_CONSTANT},
        {"else", TokType::K_ELSE},
        {"return", TokType::K_RETURN},
    };
    WordData poundwordsData[] = {
	    {"import", TokType::P_IMPORT},
    };
    HashmapStr keywords;
    HashmapStr poundwords;
    const u8 keywordCount = (u32)TokType::K_END - (u32)TokType::K_START - 1;
    const u8 poundwordCount = (u32)TokType::P_END - (u32)TokType::P_START - 1;

    void init(HashmapStr &map, WordData *data, u8 count) {
        map.init(count);
        
        for(u8 i = 0; i < count; i += 1){
            map.insertValue({(char*)data[i].str, (u32)strlen(data[i].str) }, (u16)data[i].type);
        };
    };
    void uninit(HashmapStr &map){map.uninit();};
};

bool isKeyword(TokType type){return type > TokType::K_START && type < TokType::K_END; };
bool isPoundword(TokType type){return type > TokType::P_START && type < TokType::P_END;};
bool isType(TokType type){
    return (type>TokType::K_START && type<TokType::K_PROC) || type == TokType::IDENTIFIER;
};
u32 eatUnwantedChars(char *mem, u32 x){
    while (true) {
        switch (mem[x]) {
        case ' ':
        case '\r':
        case '\t':
            x += 1;
            continue;
        case '\0':
        default:
            return x;
        };
    };
    return x;
};
b32 isAlpha(char x) { return (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z'); };
b32 isNum(char x) { return (x >= '0' && x <= '9'); };
struct Lexer {
    DynamicArray<TokenOffset> tokenOffsets;
    DynamicArray<TokType> tokenTypes;
    char *fileName;
    char *fileContent;

    bool init(char *fn){
        char tempBuff[100];
        u32 len = os::getFileFullName(fn, tempBuff);
        FILE *fp = fopen(tempBuff, "r");
        fseek(fp, 0, SEEK_END);
        u64 size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        fileName = (char*)mem::alloc(len + size + 17); //one for newline in the start, and 16 for SIMD padding(comments,etc...)
        memcpy(fileName, tempBuff, len+1);

        fileContent = fileName + len + 1;
        fileContent[0] = '\n'; //padding for getLineAndOff
        fileContent += 1;
        size = fread(fileContent, sizeof(char), size, fp);
        memset(fileContent + size, '\0', 16);

        //50% of the file size. @foodforthought: change percentage?
        u32 tokenCount = (u32)((50 * size) / 100) + 1;
        tokenTypes.init(tokenCount);
        tokenOffsets.init(tokenCount);
        return true;
    };
    void uninit(){
        mem::free(fileName);
        tokenTypes.uninit();
        tokenOffsets.uninit();
    };
    void emitErr(u64 off, char *fmt, ...) {
        if(report::errorOff == MAX_ERRORS) { return; };
        report::Report &rep = report::errors[report::errorOff];
        report::errorOff += 1;
        rep.fileName = fileName;
        rep.off = off;
        rep.fileContent = fileContent;
        if (fmt != nullptr) { rep.msg = report::reportBuff + report::reportBuffTop; };
        va_list args;
        va_start(args, fmt);
        report::reportBuffTop += vsprintf(report::reportBuff, fmt, args);
        va_end(args);
    };
    void emitWarning(u64 off, char *fmt, ...) {
        if (report::errorOff == MAX_ERRORS) { return; };
        report::Report &rep = report::warnings[report::warnOff];
        report::warnOff += 1;
        rep.fileName = fileName;
        rep.off = off;
        rep.fileContent = fileContent;
        if (fmt != nullptr) { rep.msg = report::reportBuff + report::reportBuffTop; };
        va_list args;
        va_start(args, fmt);
        report::reportBuffTop += vsprintf(report::reportBuff, fmt, args);
        va_end(args);
    };
    b32 genTokens() {
        char *src = fileContent;
        u32 x = eatUnwantedChars(src, 0);
        while (src[x] != '\0') {
            switch (src[x]) {
            case '#':{
                x += 1;
                u32 start = x;
                while(isAlpha(src[x])){
                    x += 1;
                };
                u32 type;
                if(Word::poundwords.getValue({src+start, x-start}, &type) == false){
                    emitErr(start, "Unkown poundword");
                    return false;
                };
                tokenTypes.push((TokType)type);
                TokenOffset offset;
                offset.off = start;
                offset.len = (u16)(x-start);
                tokenOffsets.push(offset);
            }break;
            case '\'':{
                u32 start = x+1;
            SINGLE_QUOTE_FIND_END:
                x += 1;
                while(src[x] != '\''){
                    x += 1;
                    if(src[x] == '\0' || src[x] == '\n'){
                        emitErr(start, "Expected ending single quotes");
                        return false;
                    };
                };
                if(src[x-1] == '\\'){goto SINGLE_QUOTE_FIND_END;};
                tokenTypes.push(TokType::SINGLE_QUOTES);
                TokenOffset offset;
                offset.off = start;
                offset.len = (u16)(x-start);
                tokenOffsets.push(offset);
                x++;
            } break;
            case '\"':{
                u32 start = x+1;
            DOUBLE_QUOTE_FIND_END:
                x += 1;
                while(src[x] != '\"'){
                    x += 1;
                    if(src[x] == '\0' || src[x] == '\n'){
                        emitErr(start, "Expected ending double quotes");
                        return false;
                    };
                };
                if(src[x-1] == '\\'){goto DOUBLE_QUOTE_FIND_END;};
                tokenTypes.push(TokType::DOUBLE_QUOTES);
                TokenOffset offset;
                offset.off = start;
                offset.len = (u16)(x-start);
                tokenOffsets.push(offset);
                x++;
            } break;
            default: {
            if (isAlpha(src[x]) || src[x] == '_') {
                u64 start = x;
                x += 1;
                while (isAlpha(src[x]) || src[x] == '_' || isNum(src[x])) { x += 1; };
                u32 type;
                if(Word::keywords.getValue({src+start, (u32)(x-start)}, &type) != false){ tokenTypes.push((TokType)type);}
                else{tokenTypes.push(TokType::IDENTIFIER);};
                TokenOffset offset;
                offset.off = start;
                offset.len = (u16)(x-start);
                tokenOffsets.push(offset);
            } else if (isNum(src[x])) {
                u64 start = x;
                TokType numType = TokType::INTEGER;
            CHECK_NUM_DEC:
                x += 1;
                while (isNum(src[x]) || src[x] == '_') { x += 1; };
                if (src[x] == '.' && src[x+1] != '.') {
                if (numType == TokType::DECIMAL) {
                    emitErr(start, "Decimal cannot have 2 decimals");
                    return false;
                };
                numType = TokType::DECIMAL;
                goto CHECK_NUM_DEC;
                };
                TokenOffset offset;
                offset.off = start;
                offset.len = (u16)(x-start);
                tokenOffsets.push(offset);
                tokenTypes.push(numType);
            } else {
                TokType type = (TokType)src[x];
                if (src[x] == '/' && src[x + 1] == '/') {
#if(SIMD)
                x += 2;
                //Since the src buffer is padded we do not have to worry
                u32 times = 0;
                char *mem = src+x;
                s32 mask;
                while (true) {
                    __m128i tocmp = _mm_set1_epi8('\n');
                    __m128i chunk = _mm_loadu_si128((const __m128i*)mem);
                    __m128i results =  _mm_cmpeq_epi8(chunk, tocmp);
                    mask = _mm_movemask_epi8(results);
                    if (mask != 0) {break;};
                    tocmp = _mm_set1_epi8('\0');
                    results =  _mm_cmpeq_epi8(chunk, tocmp);
                    mask = _mm_movemask_epi8(results);
                    if (mask == 0) {
                    mem += 16;
                    times += 16;
                    continue;
                    };
                    return true;
                };
                u32 xy = 0;
                while (IS_BIT(mask, xy) == 0) { xy += 1; };
                x += times + xy + 1;
#else
                while(src[x] != '\n'){
                    x += 1;
                    if(src[x] == '\0'){goto LEXER_EMIT_END_OF_FILE;};
                };
                x += 1;		
#endif
                x = eatUnwantedChars(src, x);
                continue;
                } else if (src[x] == '/' && src[x+1] == '*') {
#if(SIMD)
                s8 level = 1;
                u64 beg = x;
                x += 3;
                while (level != 0) {
                    __m128i tocmp = _mm_set1_epi8('/');
                    __m128i chunk = _mm_loadu_si128((const __m128i*)(src+x));
                    __m128i results =  _mm_cmpeq_epi8(chunk, tocmp);
                    s32 frontslashMask = _mm_movemask_epi8(results);
                    tocmp = _mm_set1_epi8('*');
                    results =  _mm_cmpeq_epi8(chunk, tocmp);
                    s32 startMask = _mm_movemask_epi8(results);
                    tocmp = _mm_set1_epi8('\0');
                    results =  _mm_cmpeq_epi8(chunk, tocmp);
                    s32 nullbyteMask = _mm_movemask_epi8(results);
                    s32 mask = frontslashMask | startMask | nullbyteMask;
                    if (mask == 0){
                    x += 16;
                    continue;
                    };
                    u32 y = 0;
                    while (mask != 0) {
                    while (IS_BIT(mask, y) == 0) {
                        x += 1;
                        y += 1;
                    };
                    CLEAR_BIT(mask, y);
                    y += 1;
                    switch (src[x]) {
                    case '\0': {
                        if(level == 0){
                            CLEAR_BIT(mask, y);
                        }else{
                            if(level < 0){
                                level *= -1;
                                emitErr(beg, "%d multi line comment%snot started", level, (level==1)?" ":"s ");
                            }else{
                                emitErr(beg, "%d multi line comment%snot terminated", level, (level==1)?" ":"s ");
                            };
                            return false;
                        };
                    } break;
                    case '*': {
                        x += 1;
                        if (src[x] == '/') {
                        level -= 1;
                        CLEAR_BIT(mask, y);
                        };
                    } break;
                    case '/': {
                        x += 1;
                        if (src[x] == '*') {
                        level += 1;
                        CLEAR_BIT(mask, y);
                        };
                    } break;
                    };
                    x += 1;
                    y += 1;
                    };
                };
#else
                u8 level = 1;
                u64 beg = x;
                x += 3;
                while (level != 0) {
                    switch (src[x]) {
                    case '\0': {
                        if(level < 0){
                            level *= -1;
                            emitErr(beg, "%d multi line comment%snot started", level, (level==1)?" ":"s ");
                        }else{
                            emitErr(beg, "%d multi line comment%snot terminated", level, (level==1)?" ":"s ");
                        };
                        return false;
                    } break;
                    case '*': {
                        x += 1;
                        if (src[x] == '/') { level -= 1; };
                    } break;
                    case '/': {
                        x += 1;
                        if (src[x] == '*') { level += 1; };
                    } break;
                    };
                    x += 1;
                };
#endif
                x = eatUnwantedChars(src, x);
                continue;
                };
                TokenOffset offset;
                offset.off = x;
                tokenOffsets.push(offset);
                tokenTypes.push(type);
                x += 1;
            };
            } break;
            };
            x = eatUnwantedChars(src, x);
        };
        LEXER_EMIT_END_OF_FILE:
        tokenTypes.push(TokType::END_OF_FILE);
        tokenOffsets.push( { x, 0 });
        return true;
    };
};

#if(DBG)
namespace dbg {
    void dumpLexerTokens(Lexer &lexer) {
        printf("\n[LEXER_TOKENS]");
        for (u32 x = 0; x < lexer.tokenTypes.count; x += 1) {
            printf("\n[TOKEN]\n");
            u32 line = 1;
            u32 off = 1;
            report::getLineAndOff(lexer.fileContent, lexer.tokenOffsets[x].off, line, off);
            printf("line: %d off: %d\n", line, off);
            switch (lexer.tokenTypes[x]) {
            case TokType::DOUBLE_QUOTES: {
            printf("double_quotes\n%.*s", lexer.tokenOffsets[x].len, lexer.fileContent + lexer.tokenOffsets[x].off);
            } break;
            case TokType::SINGLE_QUOTES: {
            printf("single_quotes\n%.*s", lexer.tokenOffsets[x].len, lexer.fileContent + lexer.tokenOffsets[x].off);
            } break;
            case TokType::IDENTIFIER: {
            printf("identifier\n%.*s", lexer.tokenOffsets[x].len, lexer.fileContent + lexer.tokenOffsets[x].off);
            } break;
            case TokType::INTEGER: {
            printf("integer\n%.*s", lexer.tokenOffsets[x].len, lexer.fileContent + lexer.tokenOffsets[x].off);
            } break;
            case TokType::DECIMAL: {
            printf("decimal\n%.*s", lexer.tokenOffsets[x].len, lexer.fileContent + lexer.tokenOffsets[x].off);
            } break;
            case TokType::END_OF_FILE: printf("end_of_file"); break;
            case (TokType)'\n': printf("new_line"); break;
            default:
            if(isKeyword(lexer.tokenTypes[x])){
                printf("keyword\n%.*s", lexer.tokenOffsets[x].len, lexer.fileContent + lexer.tokenOffsets[x].off);
            }else if(isPoundword(lexer.tokenTypes[x])){
                printf("poundword\n%.*s", lexer.tokenOffsets[x].len, lexer.fileContent + lexer.tokenOffsets[x].off);
            }else{
                printf("%c", lexer.tokenTypes[x]);
            };
            break;
            };
        };
        printf("\n----\n");
    };
};
#endif