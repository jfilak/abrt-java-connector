import java.io.*;
import java.net.*;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;



/**
 * Test for common issues which can be found in various applications
 * written in the Java programming language. Tested on IcedTea7-2.3
 * based on OpenJDK7u6.
 *
 * @author Pavel Tisnovsky &lt;ptisnovs@redhat.com&gt;
 *
 *
 *
 * Tested issues:
 *
 * 1) Memory related:
 *    - allocate huge amount of memory
 *
 * 2) File system related:
 *   - read from a file which does not exists
 *   - read from an unreadable file
 *   - write to an unwritable file
 *
 * 3) Network related:
 *   - read from an unknown host
 *   - read from closed socket
 *   - use of malformed URL
 *   - read non-existing content from an URL
 *
 * 4) Common exception handling:
 *   - IndexOutOfBoundsException
 *   - StringIndexOutOfBoundsException
 *   - NullPointerException
 *   - ClassCastException
 *   - ClassNotFoundException
 *
 * 5) External library:
 *   - System.load() failure
 *   - System.loadLibrary() failure
 *
 */
public class Test {



    /**
     * Used as one dimension size for dynamic array allocation
     */
    private static final int DIM_SIZE = 4096;



    /**
     * Used by network tests.
     */
    private static final int ECHO_SERVICE_SOCKET_NUMBER = 7;



    /**
     * Simulate one very common problem in some Java apps - creating huge
     * arrays (caused by some mistake in calculation of array size).
     */
    public static void allocateMemory() {
        /* allocated size = 4096*4096*4 bytes */
        { int[]      intArray1D = new int[DIM_SIZE * DIM_SIZE]; }
        /* allocated size = 4096*4096*4 bytes */
        { int[][]    intArray2D = new int[DIM_SIZE][DIM_SIZE]; }
        { double[]   doubleArray1D = new double[DIM_SIZE * DIM_SIZE]; }
        { double[][] doubleArray2D = new double[DIM_SIZE][DIM_SIZE]; }

        // string could be allocated using some base array
        { String s = new String(new byte[DIM_SIZE * DIM_SIZE]); }
    }



    /**
     * Attempt to read from a file which does not exists at all.
     */
    public static void readWrongFile() {
        File f = new File("_wrong_file_");
        try {
            FileInputStream fis = new FileInputStream(f);
            try {
                fis.read();
                fis.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
        }
        catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }



    /**
     * Attempt to read from a file which is not readable for given user.
     */
    public static void readUnreadableFile() {
        File f = new File("/root/.bashrc");
        try {
            FileInputStream fis = new FileInputStream(f);
            try {
                fis.read();
                // not a good idea to close file here
                fis.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
        }
        catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }



    /**
     * Attempt to write into a file which is not writable for given user.
     */
    public static void writeToUnwritableFile() {
        File f = new File("/root/.bashrc");
        try {
            FileOutputStream fos = new FileOutputStream(f);
            try {
                fos.write(42);
                // not a good idea to close file here
                fos.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
        }
        catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }



    /**
     * Attempt to read something from an unknown host using socket.
     */
    public static void readFromUnknownHost() {
        Socket s = null;
        try {
            s = new Socket("xyzzy", ECHO_SERVICE_SOCKET_NUMBER);
        }
        catch (UnknownHostException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }
    }



    /**
     * Attempt to read from socket which is not open.
     * Don't start echo server or if it is already
     * started, please change socket number.
     */
    public static void readFromSocket() {
        Socket s = null;
        try {
            s = new Socket("localhost", ECHO_SERVICE_SOCKET_NUMBER);
        }
        catch (UnknownHostException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }
    }



    /**
     * Attempt to use malformed URL.
     */
    public static void malformedURL() {
        try {
            URL url = new URL("@#$%^&malformed URL@#$%^&*()");
        }
        catch (MalformedURLException e) {
            e.printStackTrace();
        }
    }



    /**
     * Attempt to read content located by an URL in case the content does not exists.
     */
    public static void readFromURL() {
        HttpServer server = null;
        try {
            server = HttpServer.create(new InetSocketAddress(54321), 0);
        }
        catch (IOException e) {
            System.out.println("Cannot create testing HTTP server -> readFromURL test disabled.");
            e.printStackTrace();
            return;
        }

        server.createContext("/", new HttpHandler() {
            public void handle(HttpExchange t) {
                try {
                    String response = "Welcome Real's HowTo test page";
                    t.sendResponseHeaders(HttpURLConnection.HTTP_NOT_FOUND, 0);
                    t.getResponseBody().write(response.getBytes());
                    t.close();
                }
                catch (IOException e) {
                    System.out.println("Cannot set 404 response header");
                    e.printStackTrace();
                }
            }
        });
        server.setExecutor(null); // creates a default executor
        server.start();

        try {
            URL url = new URL("http://localhost:54321/_this_does_not_exists_");
            try {
                BufferedReader in = new BufferedReader(
                    new InputStreamReader(url.openStream()));
                String inputLine;
                while ((inputLine = in.readLine()) != null) {
                    System.out.println(inputLine);
                }
                in.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
        }
        catch (MalformedURLException e) {
            e.printStackTrace();
        }
        finally {
            server.stop(0);
        }
    }



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
     * This method throws an StringIndexOutOfBoundsException which
     * can be catched in other method.
     */
    public static void throwStringIndexOutOfBoundsException() {
        String s = "xyzzy";
        // index -1 is clearly out of range
        s.charAt(-1);
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
     * This method throws a ClassCastException which
     * can be catched in other method.
     */
    public static void throwClassCastException() {
        Object o = new Integer(0);
        // ClassCastException
        ((String)o).toString();
    }



    /**
     * Common problem in Java applications - loading
     * classes which don't exists at all or are not
     * on CLASSPATH.
     */
    public static void loadMissingClass() throws ClassNotFoundException {
        // this class should not exists
        Class c = Class.forName("xyzzy");
    }



    /**
     * Common problem in Java applications - loading
     * binary library which can not be located.
     */
    public static void loadLibrary() {
        // this library should not exists
        System.load("xyzzy");
    }



    /**
     * Common problem in Java applications - loading
     * binary library which can not be located.
     */
    public static void loadSystemLibrary() {
        // this system library should not exists
        System.loadLibrary("xyzzy");
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
     * Call other method which throws a StringIndexOutOfBoundsException
     * and catch this exception.
     */
    public static void catchStringIndexOutOfBoundsException() {
        try {
            throwStringIndexOutOfBoundsException();
        }
        catch (Throwable t) {
            t.printStackTrace();
        }
    }



    /**
     * Call other method which throws a NullPointerException
     * and catch this exception.
     */
    public static void catchNullPointerException() {
        try {
            throwNullPointerException();
        }
        catch (Throwable t) {
            t.printStackTrace();
        }
    }



    /**
     * Call other method which throws a ClassCastException
     * and catch this exception.
     */
    public static void catchClassCastException() {
        try {
            throwClassCastException();
        }
        catch (Throwable t) {
            t.printStackTrace();
        }
    }



    /**
     * Call other method which throws a ClassNotFoundException
     * and catch this exception.
     */
    public static void catchClassNotFoundException() {
        try {
            loadMissingClass();
        }
        catch (Throwable t) {
            t.printStackTrace();
        }
    }



    /**
     * Call other method which throws an UnsatisfiedLinkError
     * and catch this exception.
     */
    public static void catchUnsatisfiedLinkErrorUserLibrary() {
        try {
            loadLibrary();
        }
        catch (Throwable t) {
            t.printStackTrace();
        }
    }



    /**
     * Call other method which throws an UnsatisfiedLinkError
     * and catch this exception.
     */
    public static void catchUnsatisfiedLinkErrorSystemLibrary() {
        try {
            loadSystemLibrary();
        }
        catch (Throwable t) {
            t.printStackTrace();
        }
    }



    /**
     * Check issues related to memory management.
     */
    public static void memoryRelatedIssues() {
        System.out.println("Memory related issues");
        allocateMemory();
    }



    /**
     * Check issues related to working with files.
     */
    public static void fileRelatedIssues() {
        System.out.println("File related issues");
        readWrongFile();
        readUnreadableFile();
        writeToUnwritableFile();
    }



    /**
     * Check issues related to working with network.
     */
    public static void networkRelatedIssues() {
        System.out.println("Network related issues");
        readFromUnknownHost();
        readFromSocket();
        readFromURL();
        malformedURL();
    }



    /**
     * Try to throw and catch all tested exceptions.
     */
    public static void throwAndCatchAllExceptions() {
        System.out.println("Common exceptions");
        catchIndexOutOfBoundsException();
        catchStringIndexOutOfBoundsException();
        catchNullPointerException();
        catchClassCastException();
        catchClassNotFoundException();
        catchUnsatisfiedLinkErrorUserLibrary();
        catchUnsatisfiedLinkErrorSystemLibrary();
    }



    /**
     * Try to throw but don't catch one exception.
     */
    public static void throwAndDontCatchException() {
        throwNullPointerException();
    }



    /**
     * Entry point to this simple test.
     */
    public static void main(String args[]) {
        System.out.println("Test.java");

        memoryRelatedIssues();
        fileRelatedIssues();
        networkRelatedIssues();
        throwAndCatchAllExceptions();

        System.out.println("continue...");
        throwAndDontCatchException();

        System.exit(0);
    }
}

// finito

