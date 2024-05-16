//@ignore
#if(__clang__)
#pragma clang diagnostic ignored "-Wwritable-strings"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#pragma clang diagnostic ignored "-Wmicrosoft-goto"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#endif

#include "include.hh"

s32 main(s32 argc, char **argv){
#if(DBG)
    DEFER(printf("\nDone :)\n"));
#endif
    if(argc < 2){
        printf("no entryfile provided\n");
        return EXIT_SUCCESS;
    };
    char *inputPath = argv[1];
    char *outputPath = "out.asm";
    if(argc == 3) outputPath = argv[2];

    Word::init(Word::keywords, Word::keywordsData, ARRAY_LENGTH(Word::keywordsData));
    Word::init(Word::poundwords, Word::poundwordsData, ARRAY_LENGTH(Word::poundwordsData));
    linearDepEntities.init();
    linearDepStrings.init();

    FileEntity &mainFileEntity = linearDepEntities.newElem();
    mainFileEntity.lexer.init(inputPath);
    mainFileEntity.file.init();

    DEFER({
        for(u32 x=0; x<linearDepEntities.count; x++){
            FileEntity &fe = linearDepEntities[x];
            fe.lexer.uninit();
            fe.file.uninit();
        }
        linearDepEntities.uninit();
        linearDepStrings.uninit();
        Word::uninit(Word::keywords);
        Word::uninit(Word::poundwords);
    });

    if(!mainFileEntity.lexer.genTokens()){
        report::flushReports();
        return EXIT_SUCCESS;
    };
    if(!parseFile(mainFileEntity.lexer, mainFileEntity.file)){
        report::flushReports();
        return EXIT_SUCCESS;
    };
    for(u32 x=0; x<linearDepStrings.count; x++){
        String path = linearDepStrings[x];
        char c = path.mem[path.len];
        path.mem[path.len] = '\0';
        FileEntity &fe = linearDepEntities.newElem();
        fe.lexer.init(path.mem);
        fe.file.init();
        path.mem[path.len] = c;
        if(!fe.lexer.genTokens()){
            report::flushReports();
            return EXIT_SUCCESS;
        };
        if(!parseFile(fe.lexer, fe.file)){
            report::flushReports();
            return EXIT_SUCCESS;
        };
    };
    u32 dependencyCount = linearDepEntities.count;
    scopeOff = 0;
    globalScopes = (Scope*)mem::alloc(sizeof(Scope) * dependencyCount);
    memset(globalScopes, 0, sizeof(Scope) * dependencyCount);
    scopeAllocMem = (Scope*)mem::alloc(sizeof(Scope)*1000);
    struc.init();
    strucs.init();
    DEFER({
        strucs.uninit();
        struc.uninit();
        for(u32 x=0; x<dependencyCount; x++){
            if(globalScopes[x].vars.len != 0){globalScopes[x].uninit();};
        };
        mem::free(globalScopes);
        for(u32 x=0; x<scopeOff; x++) scopeAllocMem[x].uninit();
        mem::free(scopeAllocMem);
    });
#if(DBG)
    for(u32 x=0; x<dependencyCount; x++){
        FileEntity &fe = linearDepEntities[x];
        printf("--------------FILE: %s--------------", fe.lexer.fileName);
        //dbg::dumpLexerTokens(fe.lexer);
        dbg::dumpASTFile(fe.file, fe.lexer);
    }
#endif
    for(u32 x=dependencyCount; x > 0;){
        x -= 1;
        FileEntity &fe = linearDepEntities[x];
        if(!checkASTFile(fe.lexer, fe.file, globalScopes[x])){
            report::flushReports();
            return EXIT_SUCCESS;
        };
    };
    return EXIT_SUCCESS;
};