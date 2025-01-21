
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.TreeMap;

public class SortWrapper {

    public static void main (String[] args)
            throws Exception
    {
        BufferedReader br = new BufferedReader (new InputStreamReader (System.in));
        for (String line; (line = br.readLine ()) != null;) {
            String trimmed = line.trim ();
            if (trimmed.startsWith ("port ") && trimmed.endsWith (" (")) {
                System.out.println ("  " + trimmed);
                TreeMap<String,String> map = new TreeMap<> ();
                String endchar = null;
                int nlines = 0;
                while ((line = br.readLine ()) != null) {
                    trimmed = line.trim ();
                    if (trimmed.equals (");")) break;
                    if (endchar == null) {
                        endchar = trimmed.substring (trimmed.length () - 1);
                    }
                    if (trimmed.endsWith (endchar)) trimmed = trimmed.substring (0, trimmed.length () - 1);
                    map.put (trimmed, trimmed);
                    nlines ++;
                }
                for (String sorted : map.keySet ()) {
                    if (-- nlines > 0) sorted += endchar;
                    System.out.println ("    " + sorted);
                }
                System.out.println ("  );");
            } else {
                System.out.println (line);
            }
        }
    }
}
