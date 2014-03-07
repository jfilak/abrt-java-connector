public class DataMethodTest {
    private static String debugStringData() {
        return "42 is the reason why!";
    }

    public static void main(String[] args) {
        System.out.println("Throwing an uncaught exception ...");

        String holyGrail = null;
        holyGrail.toUpperCase();
    }
}
