#include "ExceptionCaugthInNative.h"
#include <iostream>

using std::cerr;
using std::cout;

void Java_ExceptionCaugthInNative_nativeMethod(JNIEnv* jni_env, jclass my_class)
{
    jmethodID throw_method = jni_env->GetStaticMethodID(my_class, "throwException", "()V");
    if (jni_env->ExceptionOccurred())
    {
        cerr << "An exception occurred while searching for throwException() method\n";
        jni_env->ExceptionDescribe();
        exit(69);
    }

    if (NULL == throw_method)
    {
        cerr << "Could not get throwException() method\n";
        exit(69);
    }

    jni_env->CallStaticObjectMethod(my_class, throw_method);
    if (!jni_env->ExceptionOccurred())
    {
        cerr << "Did not get the expected exception\n";
        exit(69);
    }

    jni_env->ExceptionClear();
}
