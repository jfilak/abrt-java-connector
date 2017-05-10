import java.io.*;
import java.net.*;
import java.lang.Class;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;


import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;

class JarGetter implements HttpHandler {

    private String jarPath;

    public JarGetter(String jarPath) {
        this.jarPath = jarPath;
    }

    public void handle(HttpExchange t) {
        try {
            FileInputStream jarStream = new FileInputStream(this.jarPath);
            t.sendResponseHeaders(200, jarStream.getChannel().size());
            int read = 0;
            byte[] buffer = new byte[1024];
            OutputStream os = t.getResponseBody();
            try {
                while(-1 != (read = jarStream.read(buffer, 0, 1024))) {
                    os.write(buffer, 0, read);
                }
            }
            finally {
                os.close();
            }
        }
        catch(IOException ex) {
            System.out.println(ex.getMessage());
        }
    }
}


/**
 * Test for common issues which can be found in various applications
 * written in the Java programming language. Tested on IcedTea7-2.3
 * based on OpenJDK7u6.
 *
 * @author Pavel Tisnovsky &lt;ptisnovs@redhat.com&gt;
 * @author Jakub Filak &lt;jfilak@redhat.com&gt;
 */
public class RemoteTest {
    /**
     * Entry point to this simple test.
     */
    public static void main(String args[]) throws IOException, NoSuchMethodException, MalformedURLException, IllegalAccessException, ClassNotFoundException, InvocationTargetException {
        String testClassName = "SimpleTest";
        Class testClassInstance = null;

        if (args.length == 2) {
            testClassName = args[1];
        }
        else if (args.length > 2) {
            System.out.println("Accepts either none or one argument.");
            System.exit(1);
        }

        HttpServer server = HttpServer.create(new InetSocketAddress(54321), 0);
        server.createContext("/", new JarGetter(args[0]));
        server.setExecutor(null); // creates a default executor
        server.start();

        /* Download the jar in order to fully intialize the second thread.
         * Since Java-1.8 HttpExchange.sendResponseHeaders() adds 'Date' header
         * with value formated according to locale. Getting the locale leads to
         * loading of some other class which results in a deadlock.
         */
        URL remoteJarUrl = new URL("http://localhost:54321/JarTest.jar");
        InputStream input = remoteJarUrl.openStream();
        input.close();

        try {
            Method method = URLClassLoader.class.getDeclaredMethod("addURL", new Class[]{URL.class});
            method.setAccessible(true);
            method.invoke(ClassLoader.getSystemClassLoader(), new Object[]{remoteJarUrl});
            /* Loaded these classes into cache. */
            final String needed[] = {"SimpleTest", "ThreadUncaughtException", "ThreadCaughtException"};
            for (String requiredClass : needed) {
                if (null == Class.forName(requiredClass)) {
                    System.out.println("Cannot get required class: " + requiredClass);
                    System.exit(1);
                }
            }

            testClassInstance = Class.forName(testClassName);
        }
        finally {
            server.stop(0);
        }

        if (null == testClassInstance) {
            System.out.println("Cannot get " + testClassName + ".class ...");
            System.exit(1);
        }

        System.out.println("RemoteTest.java " + args[0]);
        testClassInstance.getMethod("throwAndCatchAllExceptions").invoke(null);
        System.out.println("continue...");
        testClassInstance.getMethod("throwAndDontCatchException").invoke(null);
        System.out.println("If everything works we should not see this message :)");
        System.exit(0);
    }
}

// finito

