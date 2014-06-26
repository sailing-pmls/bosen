We have built Java program by gradle tool, which needs to be installed first.

1. Build C++ and Java program
$gradle build

If the API of petuum has changed, the java interface needs to be updated by:
$gradle generateNativeHeaders

2. Run the demo of java interface
We have written a demo program to despatch dependencies and run the java program on different nodes. In our despatcher, SSH and SCP are applied to send programs and data. There are two kinds of authentication for SSH access: 
	1)Access with account and password: 
		User needs to setup the account and password by DeployTask.setUsernameAndPassword() 
	2)Access with the authorized public key:
		If user has copied the public key into the authoried key list in the remote machine, this kind of authentication is more convenient. To use the public key authentication, user needs to setup the username and the path of private key file(Attention: this is "private", not "public") or use a default one(~/.ssh/id_rsa) by DeployTask.setPublicKeyAuthentication.
		Currently, the supported type of key are only DSA or RSA. The ECDSA is not supported yet.

For authorized access, user needs to setup the account in our demo program(Test.java).

As "unzip" is used in our despatcher, user needs to install "unzip" in every machine.

After building, our demo program in src/main/java/com/petuum/app/Test.java can be executed by a Java IDE or by running: $java -classpath build/libs/petuum-0.21.jar:jar/ganymed-ssh2-build210.jar com.petuum.app.Test

Additional Tools:
1. gradle-1.12
2. swig-3.0.2
3. jdk-7u55

