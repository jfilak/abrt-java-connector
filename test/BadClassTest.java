import java.io.*;
import java.net.*;



public class BadClassTest {

    /**
     * Entry point to this simple test.
     */
    public static void main(String args[]) throws ClassNotFoundException {
        System.out.println("BadClassTest.java");
        Class.forName("foobar");
        System.exit(0);
    }
}

// finito

