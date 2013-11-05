import java.io.*;
import java.util.*;
import java.net.*;


/**
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */

class ThreadCaughtException extends Thread {
    private void level_three() {
        SimpleTest.throwAndCatchAllExceptions();
    }

    private void level_two() {
        try {
            Thread.currentThread().sleep(5);
        }
        catch (InterruptedException ex) {
            System.out.println("Interrupted");
        }
        level_three();
    }

    private void level_one() {
        try {
            Thread.currentThread().sleep(5);
        }
        catch (InterruptedException ex) {
            System.out.println("Interrupted");
        }
        level_two();
    }

    public void run() {
        level_one();
    }
}

public class ThreadStressTest {
    /**
     * Entry point to this multi thread test.
     */
    public static void main(String args[]) {
        System.out.println("Test.java");

        List<Thread> tojoin = new LinkedList<Thread>();

        for (int i = 100; i != 0; --i) {
            for (int j = 300; j != 0; --j) {
                Thread t = new ThreadCaughtException();
                tojoin.add(t);
                System.out.println("Starting Thread: " + Integer.toString((i * j) + j));
                t.start();
            }

            try {
                Thread.currentThread().sleep(1000);
            }
            catch (InterruptedException ex) {
                System.out.println("Interrupted");
            }
        }

        for (Thread t : tojoin) {
            try {
                t.join();
            }
            catch(InterruptedException ex) {
                System.err.println("Can't join a thread because thread join() was interrupted.");
            }
        }

        System.exit(0);
    }
}

// finito

