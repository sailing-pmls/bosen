%module petuum
%include "std_vector.i"
%include "std_string.i"
%include "std_map.i"
%include "stdint.i"
%include "cpointer.i"
%include "various.i"

%include "dense_row.i"
%include "row_access.i"
%include "table.i"
%include "table_group.i"
%include "host_info.i"
%include "utils.i"

%pragma(java) jniclasscode=%{
    static {
        try {
            String path = System.getProperty("user.dir") + "/build/libs/libpetuum_ps.so";
            if(!new java.io.File(path).exists()) {
                path = System.getProperty("user.dir") + "libpetuum_ps.so";
            }
            if(new java.io.File(path).exists()) {
                System.load(path);
            } else {
                System.loadLibrary("petuum_ps");
            }

        } catch (UnsatisfiedLinkError e) {
            System.err.println("Native code library failed to load. \n" + e);
            System.exit(1);
        }
    }
%}