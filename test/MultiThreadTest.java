import java.io.*;
import java.net.*;


/**
 * @author Pavel Tisnovsky &lt;ptisnovs@redhat.com&gt;
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */

public class MultiThreadTest {

    public static void throwAndCatchAllExceptions()
    {
        runThread(new ThreadCaughtException());
    }

    public static void throwAndDontCatchException()
    {
        runThread(new ThreadUncaughtException());
    }

   public static void runThread(Thread t) {
        t.start();
        try {
            t.join();
        }
        catch(InterruptedException ex) {
            System.err.println("Can't join a thread because thread join() was interrupted.");
        }
    }

    /**
     * Entry point to this multi thread test.
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

