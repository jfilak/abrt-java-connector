package com.redhat.abrt.test;



/**
 * An exception class with very long name for test of message shortening
 * algorithm.
 *
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */
class UnbelievableLongJavaClassNameException extends RuntimeException {
    public UnbelievableLongJavaClassNameException() { super(); }
    public UnbelievableLongJavaClassNameException(String message) { super(message); }
    public UnbelievableLongJavaClassNameException(String message, Throwable cause) { super(message, cause); }
    public UnbelievableLongJavaClassNameException(Throwable cause) { super(cause); }
}



/**
 * A class with very long name, method names and throwing an exception with
 * very long name for test of message shortening algorithm.
 *
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */
public class UnbelievableLongJavaClassNameHavingExtremlyLongAndSenselessMethodNames
{
    public static void oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenSeventeenEighteenNineteenTwentyTwentyOneTwentyTwooTwentyThreeTwentyFourTwentyFiveTwentySixTwentySevenTwentyEightTwentyNineUpToOneHundredThousand()
    {
        throw new UnbelievableLongJavaClassNameException();
    }

    public static void oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenSeventeenEighteenNineteenTwentyUpToOneHundredThousand()
    {
        throw new UnbelievableLongJavaClassNameException();
    }

    public static void oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenSeventeenUpToOneHundredThousand()
    {
        throw new UnbelievableLongJavaClassNameException();
    }

    public static void oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenUpToOneHundredThousand()
    {
        throw new UnbelievableLongJavaClassNameException();
    }

    /**
     * Entry point to this shortening test.
     */
    public static void main(String args[]) {
        System.out.println("Long names handling started");
        if (args.length == 0) {
            System.out.println("You better pass an argument from [0, 1, 2]");
            oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenSeventeenEighteenNineteenTwentyUpToOneHundredThousand();
        }
        else {
            switch(args[0]) {
                case "0":
                    System.out.println("Last Twenty Nine");
                    oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenSeventeenEighteenNineteenTwentyTwentyOneTwentyTwooTwentyThreeTwentyFourTwentyFiveTwentySixTwentySevenTwentyEightTwentyNineUpToOneHundredThousand();
                    break;
                case "1":
                    System.out.println("Last Twenty");
                    oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenSeventeenEighteenNineteenTwentyUpToOneHundredThousand();
                    break;
                case "2":
                    System.out.println("Last Seventeen");
                    oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenSeventeenUpToOneHundredThousand();
                    break;
                case "3":
                    System.out.println("Last Sixteen");
                    oneTwoThreeFroFiveSixSevenEightNineTenElevenTwelveThirteenSixteenUpToOneHundredThousand();
                    break;
                default:
                    System.err.println("Argument is not in range [0, 1, 2]: " + args[0]);
                    System.exit(1);
            }
        }

        System.exit(0);
    }
}
