package com.petuum.app;

//import com.petuum.petuum_ps.TableGroup;

import java.io.IOException;

/**
 * Created by suyuxin on 14-6-9.
 */
public class Test {

    static public void main(String[] args) throws IOException {
        DeployTask deployer = new DeployTask("machinefiles/two_nodes");
        //authenticate:
        deployer.setUsernameAndPassword("suyuxin", "123");
        //or:
        deployer.setPublicKeyAuthentication("suyuxin", "/home/suyuxin/.ssh/id_rsa");
        //or:
        deployer.setPublicKeyAuthentication("suyuxin", null);//will employ the default position of RSA
        //run
        deployer.deploy("com.petuum.app.HelloWorld", "test");
    }
}
