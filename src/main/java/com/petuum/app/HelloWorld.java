package com.petuum.app;

import com.petuum.petuum_ps.*;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class HelloWorld {
    public static void main(String[] args) throws InterruptedException{
        Logger log = LogManager.getLogger(HelloWorld.class.getName());

        TableGroupConfig table_group_config = new TableGroupConfig();
        Map<Integer, ClientTableConfig> client_table_config_map = new HashMap<Integer, ClientTableConfig>();
        XMLparser.parseTableConfigs("config/table_HelloWorld.xml", table_group_config, client_table_config_map);
        petuum.GetHostInfos(args[1], table_group_config.getHost_map());
        petuum.GetServerIDsFromHostMap(table_group_config.getServer_ids(),
                table_group_config.getHost_map());
        table_group_config.setClient_id(Integer.valueOf(args[0]));
        
        // Configure PS row types
		TableGroup.RegisterDenseFloatRow(0);         //dense row
        // Start PS
        // IMPORTANT: This command starts up the name node service on client 0.
        //            We therefore do it ASAP, before other lengthy actions like
        //            loading data.
		TableGroup.Init(table_group_config, false);

        // Configure PS tables
        Iterator iter=client_table_config_map.entrySet().iterator();
        while(iter.hasNext()){
            Map.Entry<Integer, ClientTableConfig> entry = (Map.Entry<Integer, ClientTableConfig>) iter.next();
            log.info("Create table ID : " + entry.getKey());
            TableGroup.CreateTable(entry.getKey(), entry.getValue());
        }
        // Finished creating tables
        TableGroup.CreateTableDone();
        log.info("Table create done!");
        //Thread operations
        ExecutorService threadPool =
                Executors.newFixedThreadPool(table_group_config.getNum_local_app_threads()-1);
        for(int i=0; i<table_group_config.getNum_local_app_threads()-1; i++){
            threadPool.execute(new threadTask(i));
        }
        threadPool.shutdown();
        while (!threadPool.isTerminated()) {}

        log.info("Hello world!");
    }
}

class threadTask implements Runnable {
    private int thread_id;
    public threadTask(int index) {
        // TODO Auto-generated constructor stub
        thread_id = index;
    }
    @Override
    public void run() {
        // TODO Auto-generated method stub
		Logger log = LogManager.getLogger(HelloWorld.class.getName());
        TableGroup.RegisterThread();
		TableFloat my_table = TableGroup.GetTableOrDieFloat(0);
		float a;
        RowAccessor acc = new RowAccessor();
        DenseFloatRow row;
        Random ran = new Random(System.nanoTime());

        log.info("<========  iteration 0 starts  =========>");
		my_table.Inc(0, 0, 7.7f);
		my_table.Inc(0, 5, 2.2f);
        if (thread_id == 0){
            try {
                log.info("sleeping...");
                Thread.sleep(5000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        TableGroup.Clock();
		log.info("<=========  iteration 0 ends  ==========>");

		log.info("<========  iteration 1 starts  =========>");
        my_table.Get(0, acc);           //wait here!

        row = acc.GetDenseFloatRow();
        a = row.get(0);
        log.info("(0, 0) = " + a);

        my_table.Inc(0, 5, -2f);
        if (thread_id == 0){
            try {
                log.info("sleeping...");
                Thread.sleep(5000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        TableGroup.Clock();
		log.info("<=========  iteration 1 ends  ==========>");
		log.info("<========  iteration 2 starts  =========>");
        my_table.Get(0, acc);           //wait here!
        row = acc.GetDenseFloatRow();
        a = row.get(5);
        log.info("(0, 5) = "+a);

		my_table.Inc(0, 2, 1.1f);
        if (thread_id == 0){
            try {
                log.info("sleeping...");
                Thread.sleep(5000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        TableGroup.Clock();
		log.info("<=========  iteration 2 ends  ==========>");
        my_table.Get(0, acc);           //wait here!
        row = acc.GetDenseFloatRow();
		a = row.get(2);
		log.info("(0, 2) = "+a);
        log.info("Thread " + thread_id + " finished!");
        TableGroup.DeregisterThread();

    }

}
