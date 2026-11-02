package com.gigantelabs.tests;
import android.content.Context;

public final class ResourceDriverHarness {
    public static final class ServiceHelper {
        public static String state = "inactive";
        public static int starts = 0;
        public static int stops = 0;
        public static boolean start(Context context) { ++starts; state = "pending"; return true; }
        public static boolean stop(Context context) { ++stops; state = "stopPending"; return true; }
        public static String[] snapshot() { return new String[] {state, ""}; }
    }
    public static final class WakeHelper {
        public static String state = "inactive";
        public static boolean throwRelease = false;
        public static boolean acquire(Context context, String tag) { state = "active"; return true; }
        public static boolean release(Context context) {
            if (throwRelease) throw new SecurityException("fixture release denied");
            state = "inactive"; return true;
        }
        public static String[] snapshot() { return new String[] {state, ""}; }
    }
    private static native void nativeRun(Context context, Class<?> service, Class<?> wake);
    public static void main(String[] args) {
        System.load(args[0]);
        nativeRun(new Context(), ServiceHelper.class, WakeHelper.class);
        System.out.println("JNI resource driver fixture tests passed");
    }
}
