foo :: struct{
    x: s32
    y: s32
}
goo :: struct{
    f: foo
}

x: u32 = 2

foo :: proc(x: s32) -> (u32){
    y: u32 = 4
}