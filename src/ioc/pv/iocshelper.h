#ifndef IOCSHELPER_H
#define IOCSHELPER_H
/** Helper for exposing functions and variables in the IOC shell
 *
 * Limitations:
 * - supports functions with up to 4 arguments
 * - argument and variable types must be: int, double, char*, or const char*
 *
 @code
 #include <pv/iocshelper.h>
 void fn0() {}
 void fn2(int a, const char *b) {}
 int var;
 void myRegistrar() {
   epics::iocshRegister<&fn0>("fn0");
   epics::iocshRegister<int, const char*, &fn2>("fn0", "a description", "b description");
   epics::iocshVariable<int, &var>("var");
 }
 extern "C" {
   epicsExportRegistrar(myRegistrar);
 }
 @endcode
 */

#include <string>

#include <iocsh.h>

namespace epics {
namespace detail {

template<typename T>
struct getarg {};
template<> struct getarg<int> {
    static int op(const iocshArgBuf& a) { return a.ival; }
    enum { argtype = iocshArgInt };
};
template<> struct getarg<double> {
    static double op(const iocshArgBuf& a) { return a.dval; }
    enum { argtype = iocshArgDouble };
};
template<> struct getarg<char*> {
    static char* op(const iocshArgBuf& a) { return a.sval; }
    enum { argtype = iocshArgString };
};
template<> struct getarg<const char*> {
    static const char* op(const iocshArgBuf& a) { return a.sval; }
    enum { argtype = iocshArgString };
};


template<int N>
struct iocshFuncInfo{
    iocshFuncDef def;
    std::string name;
    iocshArg *argarr[N+1];
    iocshArg args[N+1];
    std::string argnames[N+1];
    iocshFuncInfo(const std::string& n) :name(n) {
        def.name = name.c_str();
        def.nargs = N;
        def.arg = (iocshArg**)&argarr;
        for(size_t i=0; i<N; i++)
            argarr[i] = &args[i];
    }
    template<int n, typename T>
    void set(const char *name) {
        argnames[n] = name;
        args[n].name = argnames[n].c_str();
        args[n].type = (iocshArgType)detail::getarg<T>::argtype;
    }
};

template<void (*fn)()>
static void call0(const iocshArgBuf *args)
{
    fn();
}

template<typename A, void (*fn)(A)>
static void call1(const iocshArgBuf *args)
{
    fn(getarg<A>::op(args[0]));
}

template<typename A, typename B, void (*fn)(A,B)>
static void call2(const iocshArgBuf *args)
{
    fn(getarg<A>::op(args[0]),
       getarg<B>::op(args[1]));
}

template<typename A, typename B, typename C, void (*fn)(A,B,C)>
static void call3(const iocshArgBuf *args)
{
    fn(getarg<A>::op(args[0]),
       getarg<B>::op(args[1]),
       getarg<C>::op(args[2]));
}

} // namespace detail


template<void (*fn)()>
void iocshRegister(const char *name)
{
    static detail::iocshFuncInfo<0> info(name);
    iocshRegister(&info.def, &detail::call0<fn>);
}

template<typename A, void (*fn)(A)>
void iocshRegister(const char *name, const char *arg1name)
{
    static detail::iocshFuncInfo<1> info(name);
    info.set<0,A>(arg1name);
    iocshRegister(&info.def, &detail::call1<A, fn>);
}

template<typename A, typename B, void (*fn)(A,B)>
void iocshRegister(const char *name,
                   const char *arg1name,
                   const char *arg2name)
{
    static detail::iocshFuncInfo<2> info(name);
    info.set<0,A>(arg1name);
    info.set<1,B>(arg2name);
    iocshRegister(&info.def, &detail::call2<A, B, fn>);
}

template<typename A, typename B, typename C, void (*fn)(A,B,C)>
void iocshRegister(const char *name,
                   const char *arg1name,
                   const char *arg2name,
                   const char *arg3name)
{
    static detail::iocshFuncInfo<3> info(name);
    info.set<0,A>(arg1name);
    info.set<1,B>(arg2name);
    info.set<2,C>(arg3name);
    iocshRegister(&info.def, &detail::call3<A, B, C, fn>);
}

template<typename V, V* addr>
void iocshVariable(const char *name)
{
    static iocshVarDef def[2];
    def[0].name = name;
    def[0].pval = (void*)addr;
    def[0].type = (iocshArgType)detail::getarg<V>::argtype;
    def[1].name = NULL;
    iocshRegisterVariable(def);
}

} // namespace epics

#endif // IOCSHELPER_H
