/**
 * Tests if abrt-java-connector generates a stack trace for exception causes.
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */
public class InnerExceptions {

    public static void run()
    {
        try {
            System.out.println("TryFinallyTest.java exceptions inside try without catch and with finally");
            SimpleTest.throwAndCatchAllExceptions();
            System.out.println("continue...");
            SimpleTest.throwAndDontCatchException();
            System.out.println("A message after an uncaught exception");
        }
        catch (java.lang.NullPointerException ex){
            throw new RuntimeException(ex);
        }
        finally {
            System.out.println("finally block");
        }
    }

    /**
     * Entry point to this TryFinally test.
     */
    public static void main(String args[]) {
        run();
        System.exit(0);
    }
}

// finito


