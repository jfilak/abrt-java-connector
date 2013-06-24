/**
 * Tests if abrt-java-connector doesnt call overriden equals method
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */



class MaliciousEqualsException extends RuntimeException {

    public MaliciousEqualsException() { super(); }
    public MaliciousEqualsException(String message) { super(message); }
    public MaliciousEqualsException(String message, Throwable cause) { super(message, cause); }
    public MaliciousEqualsException(Throwable cause) { super(cause); }

    @Override
    public boolean equals(Object other) {
        System.out.println("Must not be called from abrt-java-connector");
        return false;
    }
}



public class OverridenEqualExceptionTest {

    public static void run() {
        throw new MaliciousEqualsException("Really ugly exception!");
    }

    /**
     * Entry point to this TryFinally test.
     */
    public static void main(String args[]) {
        try {
            run();
        }
        finally {
            System.out.println("finally ...");
        }
        System.exit(0);
    }
}

// finito

