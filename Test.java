public class Test {
    public static final int BAR = 3;
    String foo = "foo";
    int bar = BAR;
    long x = 0xdeadbeefcafeL;
    double y = 123.456D;
    float z = 1.25F;

    public static void main(String[] args) {
        try {
            throw new Exception("test exception");
        } catch (Exception e) {
            System.out.println("caught " + e.getMessage());
        }
    }
}

/* vi: set et ts=4 sw=4: */
