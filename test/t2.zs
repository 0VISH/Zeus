foo :: struct{
    x: s32
    y: s32
}
goo :: struct{
    f: foo
}

foo :: proc(x: s32 = 4) -> (u32){
    y: u32 = 4
}