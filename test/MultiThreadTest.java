import java.io.*;
import java.net.*;
//import SimpleTest;


/**
 * @author Pavel Tisnovsky &lt;ptisnovs@redhat.com&gt;
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */

class ThreadCaughtException extends Thread {
    public void run() {
        SimpleTest.throwAndDontCatchException();
    }
}

class ThreadUncaughtException extends Thread {
    public void run() {
        SimpleTest.throwAndDontCatchException();
    }
}

public class MultiThreadTest {
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
        runThread(new ThreadCaughtException());
        System.out.println("continue...");
        runThread(new ThreadUncaughtException());
        System.exit(0);
    }
}

// finito

