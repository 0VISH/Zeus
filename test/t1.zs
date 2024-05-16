foo :: struct{
    x: s32
    y: s32
}
goo :: struct{
    f: foo
}

g: goo
g.f.x = 2