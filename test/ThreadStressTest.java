import java.io.*;
import java.util.*;
import java.util.regex.*;
import java.net.*;


/**
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */

class StressThreadCaughtException extends Thread {
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
        int repeats = 60;
        int threads = 600;

        for (String arg : args) {
            Scanner s = new Scanner(arg);
            s.findInLine("^([^=]+)=(\\d+)$");
            MatchResult r = s.match();
            if (r.groupCount() != 2) {
                System.err.println("Invalid argument format [reps|threads=number]: '" + arg + "'");
                System.exit(1);
            }
            switch (r.group(1)) {
                case "reps":
                    repeats = Integer.parseInt(r.group(2));
                    break;
                case "threads":
                    threads = Integer.parseInt(r.group(2));
                    break;
                default:
                    System.err.println("Unknown argument '" + r.group(1) + "'");
                    System.exit(1);
                    break;
            }
        }

        System.out.println("Test.java");

        List<Thread> tojoin = new LinkedList<Thread>();
        for (int i = repeats; i != 0; --i) {
            for (int j = threads; j != 0; --j) {
                try {
                    Thread t = new StressThreadCaughtException();
                    tojoin.add(t);
                    System.out.println("Starting Thread: " + Integer.toString((i * j) + j));
                    t.start();
                }
                catch(Throwable t) {
                    System.out.println("Thread start: " + t.toString());
                    System.exit(1);
                }
            }

            try {
                Thread.currentThread().sleep(500);
            }
            catch (InterruptedException ex) {
                System.out.println("Interrupted");
            }
        }

        System.out.println("All Threads Started");
        for (Thread t : tojoin) {
            try {
                t.join();
            }
            catch(InterruptedException ex) {
                System.err.println("Can't join a thread because thread join() was interrupted.");
            }
        }

        System.out.println("All Threads Finished");
        System.exit(0);
    }
}

// finito

