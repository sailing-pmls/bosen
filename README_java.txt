We have built Java program by gradle tool, which needs to be installed first(gradle >= 1.12).

1. Build C++ and Java program
$gradle build

If the API of petuum has changed, the java interface needs to be updated by:
$gradle generateNativeHeaders

2. Run the demo of java interface
We have written a demo program to despatch dependencies and run the java program on different nodes. In our despatcher, SSH and SCP are applied to send programs and data. For authorized access, users need to create account called "petuum" with password "123456" in those nodes.

After building, our demo program in src/main/java/petuum/app/Test.java can be executed by a Java IDE or by running: $java -classpath build/libs/petuum-0.21.jar:lib/ganymed-ssh2-build210.jar com.petuum.app.Test

