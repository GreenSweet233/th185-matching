#include "FieldAccess.hpp"

// 本文件的 struct 在 FieldAccess.hpp 里定义于**全局作用域**（见该文件说明），
// 所以成员函数定义也不要套 namespace —— 否则 clang 报
// "namespace 'th185' does not enclose namespace 'ObjField2A0'"。

//@witness func=0x00404D10 kind=stdcall
int __stdcall first_of_three(int a, int b, int c) {
    (void)b;
    (void)c;
    return a;
}

//@witness func=0x00407E40 kind=thiscall
int ObjField2A0::is_set() { return f_2A0 != 0; }

//@witness func=0x0040A050 kind=thiscall
ObjClearFirst* ObjClearFirst::clear_and_return_this() {
    f_00 = 0;
    return this;
}

//@witness func=0x00409F60 kind=thiscall
void ObjField20::set(int value) { f_20 = value; }

//@witness func=0x004063C0 kind=thiscall
void ObjField5F0::copy_from(const Vec3Raw* source) { f_5F0 = *source; }

//@witness func=0x00405A60 kind=thiscall
__m128 __fastcall ObjSse44Load::load(ObjSse44Load* self) {
    return _mm_load_ss(&self->f_44);
}

//@witness func=0x0041C710 kind=thiscall
__m128 __fastcall ObjMul7454::mul(ObjMul7454* self) {
    return _mm_mul_ss(_mm_load_ss(&self->f_74),
                      *reinterpret_cast<__m128*>(&self->f_54));
}

//@witness func=0x0048F711 kind=cdecl
void __cdecl forward_to_cpu_probe(int features) {
    th185_CpuFeatureProbe(features);
}
