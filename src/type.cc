#pragma once

enum class Type{
    INVALID,
    BOOL,
    F64,
    S64,
    U64,
    F32,
    S32,
    U32,
    S16,
    U16,
    CHAR,
    S8,
    U8,
    COMP_DECIMAL,
    COMP_INTEGER,
    COMP_STRING,
    COUNT,
};

bool isNumber(Type type){
    return type >= Type::S64 && type <= Type::COMP_INTEGER && type != Type::CHAR;
}