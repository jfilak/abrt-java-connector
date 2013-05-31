import java.io.*;
import java.net.*;
import java.lang.Class;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;



/**
 * Test for common issues which can be found in various applications
 * written in the Java programming language. Tested on IcedTea7-2.3
 * based on OpenJDK7u6.
 *
 * @author Pavel Tisnovsky &lt;ptisnovs@redhat.com&gt;
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */
public class RemoteTest {

    /**
     * Entry point to this simple test.
     */
    public static void main(String args[]) throws NoSuchMethodException, MalformedURLException, IllegalAccessException, ClassNotFoundException, InvocationTargetException {
        Method method = URLClassLoader.class.getDeclaredMethod("addURL", new Class[]{URL.class});
        method.setAccessible(true);
        method.invoke(ClassLoader.getSystemClassLoader(), new Object[]{new URL(args[0])});
        Class simpleTestClass = Class.forName("SimpleTest");

        System.out.println("RemoteTest.java " + args[0]);
        simpleTestClass.getMethod("throwAndCatchAllExceptions").invoke(null);
        System.out.println("continue...");
        simpleTestClass.getMethod("throwAndDontCatchException").invoke(null);

        System.exit(0);
    }
}

// finito

