
import java.io.BufferedReader;
import java.io.EOFException;
import java.io.InputStreamReader;
import java.util.LinkedList;
import java.util.TreeMap;

public class SortJSon {

    public static BufferedReader br;
    public static int lineno = 1;
    public static int charno;

    public static void main (String[] args)
            throws Exception
    {
        br = new BufferedReader (new InputStreamReader (System.in));
        try {
            JSonValue topvalue = readJSonValue ();
            topvalue.print ("");
            System.out.println ("");
        } catch (Exception e) {
            System.err.println ("exception processing line " + lineno + " char " + charno);
            System.err.println (e.toString ());
        }
    }

    public static JSonValue readJSonValue ()
            throws Exception
    {
        char c = readNonSpace ();
        if (c == '{') {
            JSonStruct jsonstruct = new JSonStruct ();
            c = readNonSpace ();
            if (c != '}') {
                pushReadBack (c);
                while (true) {
                    JSonString membername = readJSonString ();
                    c = readNonSpace ();
                    if (c != ':') throw new Exception ("expecting : between member name and value");
                    JSonValue membervalue = readJSonValue ();
                    jsonstruct.members.put (membername.valwithquotes, membervalue);
                    c = readNonSpace ();
                    if (c == '}') break;
                    if (c != ',') throw new Exception ("expecting , or } after member value");
                }
            }
            return jsonstruct;
        }
        if (c == '[') {
            JSonArray jsonarray = new JSonArray ();
            c = readNonSpace ();
            if (c != ']') {
                pushReadBack (c);
                while (true) {
                    JSonValue elementvalue = readJSonValue ();
                    jsonarray.elements.addLast (elementvalue);
                    c = readNonSpace ();
                    if (c == ']') break;
                    if (c != ',') throw new Exception ("expecting , or } after element value");
                }
            }
            return jsonarray;
        }
        if (c == '"') {
            pushReadBack (c);
            return readJSonString ();
        }
        pushReadBack (c);
        return readJSonSymbol ();
    }

    public static char readNonSpace ()
            throws Exception
    {
        char c;
        do c = readChar ();
        while (c <= 32);
        return c;
    }

    public static int readBackChar = -1;

    public static void pushReadBack (char c)
    {
        readBackChar = c;
    }

    public static char readChar ()
            throws Exception
    {
        int c = readBackChar;
        if (c >= 0) {
            readBackChar = -1;
        } else {
            charno ++;
            c = br.read ();
            if (c < 0) throw new EOFException ();
            if (c == '\n') {
                lineno ++;
                charno = 0;
            }
        }
        return (char) c;
    }

    public static JSonString readJSonString ()
            throws Exception
    {
        char c = readNonSpace ();
        if (c != '"') throw new Exception ("expected \" for string, had " + c);
        StringBuilder sb = new StringBuilder ();
        sb.append (c);
        while (true) {
            c = readChar ();
            sb.append (c);
            if (c == '"') break;
            if (c == '\\') {
                sb.append (readChar ());
            }
        }
        JSonString jsonstring = new JSonString ();
        jsonstring.valwithquotes = sb.toString ();
        return jsonstring;
    }

    public static JSonSymbol readJSonSymbol ()
            throws Exception
    {
        StringBuilder sb = new StringBuilder ();
        char c = readNonSpace ();
        while (c > 32) {
            if (":\"\'{}[]".indexOf (c) >= 0) break;
            sb.append (c);
            c = readChar ();
        }
        pushReadBack (c);
        JSonSymbol jsonsymbol = new JSonSymbol ();
        jsonsymbol.name = sb.toString ();
        return jsonsymbol;
    }

    public static abstract class JSonValue {
        public abstract void print (String indent);
    }

    public static class JSonArray extends JSonValue {
        public LinkedList<JSonValue> elements = new LinkedList<> ();

        @Override
        public void print (String indent)
        {
            if (elements.size () == 0) {
                System.out.print ("[ ]");
            } else {
                System.out.print ("[");
                String indented = indent + "  ";
                boolean first = true;
                for (JSonValue value : elements) {
                    if (! first) System.out.print (",");
                    System.out.println ("");
                    System.out.print (indented);
                    value.print (indented);
                    first = false;
                }
                System.out.println ("");
                System.out.print (indent + "]");
            }
        }
    }

    public static class JSonString extends JSonValue {
        public String valwithquotes;

        @Override
        public void print (String indent)
        {
            System.out.print (valwithquotes);
        }
    }

    public static class JSonStruct extends JSonValue {
        public TreeMap<String,JSonValue> members = new TreeMap<> ();

        @Override
        public void print (String indent)
        {
            if (members.size () == 0) {
                System.out.print ("{ }");
            } else {
                System.out.print ("{");
                String indented = indent + "  ";
                boolean first = true;
                for (String key : members.keySet ()) {
                    if (! first) System.out.print (",");
                    System.out.println ("");
                    System.out.print (indented);
                    System.out.print (key);
                    System.out.print (": ");
                    JSonValue value = members.get (key);
                    value.print (indented);
                    first = false;
                }
                System.out.println ("");
                System.out.print (indent + "}");
            }
        }
    }

    public static class JSonSymbol extends JSonValue {
        public String name;

        @Override
        public void print (String indent)
        {
            System.out.print (name);
        }
    }
}
