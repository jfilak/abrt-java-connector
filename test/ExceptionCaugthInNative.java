public class ExceptionCaugthInNative
{
    public static void throwException()
    {
        SimpleTest.throwAndDontCatchException();
    }

    public static native void nativeMethod();

    public static void main(String[] args)
    {
        System.loadLibrary("ExceptionCaugthInNative");

        ExceptionCaugthInNative.nativeMethod();

        System.out.println("Ok");
    }
}
