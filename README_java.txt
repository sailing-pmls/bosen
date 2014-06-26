We have built Java program by gradle tool, which needs to be installed first.

1. Build C++ and Java program
$gradle build

If the API of petuum has changed, the java interface needs to be updated by:
$gradle generateNativeHeaders

2. Run the demo of java interface
We have written a demo program to despatch dependencies and run the java program on different nodes. In our despatcher, SSH and SCP are applied to send programs and data. For authorized access, user needs to setup the account in our demo program(Test.java).

As "unzip" is used in our despatcher, user needs to install "unzip" in every machine.

After building, our demo program in src/main/java/com/petuum/app/Test.java can be executed by a Java IDE or by running: $java -classpath build/libs/petuum-0.21.jar:jar/ganymed-ssh2-build210.jar com.petuum.app.Test

Additional Tools:
1. gradle-1.12
2. swig-3.0.2
3. jdk-7u55

