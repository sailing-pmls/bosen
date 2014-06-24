package com.petuum.app;

//import com.petuum.petuum_ps.TableGroup;

import java.io.IOException;

/**
 * Created by suyuxin on 14-6-9.
 */
public class Test {

    static public void main(String[] args) throws IOException {
        DeployTask deployer = new DeployTask("machinefiles/two_nodes");
        deployer.setUsernameAndPassword("petuum", "123456");
        deployer.deploy("com.petuum.app.Lasso_ps");
    }
}
