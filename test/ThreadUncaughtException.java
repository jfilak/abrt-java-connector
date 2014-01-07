/**
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */

public class ThreadUncaughtException extends Thread {
    public void run() {
        SimpleTest.throwAndDontCatchException();
    }
}
