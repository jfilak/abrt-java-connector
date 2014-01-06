/**
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */

public class ThreadCaughtException extends Thread {
    public void run() {
        SimpleTest.throwAndCatchAllExceptions();
    }
}
