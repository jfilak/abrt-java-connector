import java.io.*;
import java.net.*;



/**
 * Test for common issues which can be found in various applications
 * written in the Java programming language. Tested on IcedTea7-2.3
 * based on OpenJDK7u6.
 *
 * @author Pavel Tisnovsky &lt;ptisnovs@redhat.com&gt;
 */
public class SimpleTest {



    /**
     * This method throws an IndexOutOfBoundsException which
     * can be catched in other method.
     */
    public static void throwIndexOutOfBoundsException() {
        int[] x = new int[10];
        // index 42 is clearly out of range
        x[42] = 42;
    }



    /**
     * This method throws a NullPointerException which
     * can be catched in other method.
     */
    public static void throwNullPointerException() {
        Object o = null;
        // NPE
        o.toString();
    }



    /**
     * Call other method which throws an IndexOutOfBoundsException
     * and catch this exception.
     */
    public static void catchIndexOutOfBoundsException() {
        try {
            throwIndexOutOfBoundsException();
        }
        catch (Throwable t) {
            t.printStackTrace();
        }
    }



    /**
     * Try to throw and catch all tested exceptions.
     */
    public static void throwAndCatchAllExceptions() {
        System.out.println("Common exceptions");
        catchIndexOutOfBoundsException();
    }



    /**
     * Try to throw but don't catch one exception.
     */
    public static void throwAndDontCatchException() {
        System.out.println("Throwing a NullPointerException");
        throwNullPointerException();
    }



    /**
     * Entry point to this simple test.
     */
    public static void main(String args[]) {
        System.out.println("Test.java");
        throwAndCatchAllExceptions();
        System.out.println("continue...");
        throwAndDontCatchException();
        System.exit(0);
    }
}

// finito

