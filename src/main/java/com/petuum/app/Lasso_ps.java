package com.petuum.app;

import com.petuum.petuum_ps.*;
import org.apache.commons.math3.linear.ArrayRealVector;
import org.apache.commons.math3.linear.OpenMapRealMatrix;
import org.apache.commons.math3.linear.RealMatrix;
import org.apache.commons.math3.linear.RealVector;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.io.*;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Created by bobo on 14-6-12.
 */
public class Lasso_ps {
    public static void main(String args[]){
        Logger log = LogManager.getLogger(Lasso_ps.class.getName());

        TableGroupConfig table_group_config = new TableGroupConfig();
        //client_table_config_map: each entry is a client table config
        Map<Integer, ClientTableConfig> client_table_config_map = new HashMap<Integer, ClientTableConfig>();
        //param_map: each entry is a parameter
        Map<String, String> param_map = new HashMap<String, String>();
        XMLparser.parseTableConfigs("config/table_Lasso.xml", table_group_config, client_table_config_map);
        XMLparser.parseParam("config/param_Lasso.xml", param_map);

        table_group_config.setClient_id(Integer.parseInt(args[0]));

        // Configure PS row types
        TableGroup.RegisterDenseFloatRow(0);         //dense row
        // Start PS
        // IMPORTANT: This command starts up the name node service on client 0.
        //            We therefore do it ASAP, before other lengthy actions like
        //            loading data.
        TableGroup.Init(table_group_config, false);
        // Load data
        // Output statistics
        int num_rows = Integer.parseInt(param_map.get("num_rows"));
        int num_cols = Integer.parseInt(param_map.get("num_cols"));
        String X_file = param_map.get("input_X");
        String Y_file = param_map.get("input_Y");
        int num_iters = Integer.parseInt(param_map.get("num_iters"));
        double lambda = Double.parseDouble(param_map.get("lambda"));
        String output_file = param_map.get("output_file");
        log.info("num_rows: " + num_rows);
        log.info("num_cols: " + num_cols);
        log.info("lambda: " + lambda);
        log.info("num_iters: " + num_iters);
        log.info("input_X: " + X_file);
        log.info("input_Y: " + Y_file);


        LassoConfig lassoConfig = new LassoConfig();
        lassoConfig.lambda = lambda;
        lassoConfig.numRows = num_rows;
        lassoConfig.numCols = num_cols;
        lassoConfig.numIters = num_iters;
        lassoConfig.outfile = output_file;

        LassoProblem lassoProblem = new LassoProblem(lassoConfig, table_group_config, client_table_config_map.get(0));
        lassoProblem.readData(X_file, Y_file);
        lassoProblem.standardiseData();

        // Configure PS tables
        Iterator iter=client_table_config_map.entrySet().iterator();
        while(iter.hasNext()){
            Map.Entry<Integer, ClientTableConfig> entry = (Map.Entry<Integer, ClientTableConfig>) iter.next();
            TableGroup.CreateTable(entry.getKey(), entry.getValue());
        }
        // Finished creating tables
        TableGroup.CreateTableDone();
        log.info("Table create done!");
        //Thread operations
        ExecutorService threadPool =
                Executors.newFixedThreadPool(table_group_config.getNum_local_app_threads() - 1);
        for(int i=0; i<table_group_config.getNum_local_app_threads()-1; i++){
            threadPool.execute(new solveLasso(i, lassoProblem));
        }
        threadPool.shutdown();
        while (!threadPool.isTerminated()) {}

//        lassoProblem.outputResult(output_file);
        log.info("Lasso_ps finished!");
    }
}
class LassoConfig{
    int numRows;
    int numCols;
    int numIters;
    double lambda;
    String outfile;
}
/**
 * Lasso regression - l1-norm
 */
class LassoProblem {
    RealMatrix Xcols;            //Xcol: store the matrix X
    ArrayRealVector Ycol;       //Ycol: store the vector Y
    ArrayRealVector Residual;   //Residual: store the residual
    LassoConfig lassoConfig;
    TableGroupConfig tableGroupConfig;
    ClientTableConfig clientTableConfig;

    public LassoProblem(LassoConfig lassoConfig,
                        TableGroupConfig tableGroupConfig,
                        ClientTableConfig clientTableConfig) {
        Xcols = new OpenMapRealMatrix(lassoConfig.numRows, lassoConfig.numCols);
        Ycol = new ArrayRealVector(lassoConfig.numRows);
        Residual = new ArrayRealVector(lassoConfig.numCols);
        this.lassoConfig = lassoConfig;

        this.tableGroupConfig = tableGroupConfig;
        this.clientTableConfig = clientTableConfig;
    }

    private static Logger log = LogManager.getLogger(Lasso_ps.class.getName());

    public void readData(String x_file, String y_file) {
        try {
            BufferedReader br_x = new BufferedReader(new FileReader(x_file));
            BufferedReader br_y = new BufferedReader(new FileReader(y_file));
            String strLine;
            while ((strLine = br_x.readLine()) != null) {
                String numArr[] = strLine.split(" ");
                Xcols.setEntry(Integer.parseInt(numArr[0]),
                        Integer.parseInt(numArr[1]),
                        Double.parseDouble(numArr[2]));
            }
            br_x.close();
            while ((strLine = br_y.readLine()) != null) {
                String numArr[] = strLine.split(" ");
                Ycol.setEntry(Integer.parseInt(numArr[0]),
                        Double.parseDouble(numArr[2]));
            }
            br_y.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public void standardiseData() {
        for(int i = 0; i < Xcols.getColumnDimension(); i++){
            double mean = 0.0;
            long nonzero = 0;
            RealVector x_col = Xcols.getColumnVector(i);
            for (int j = 0; j < x_col.getDimension(); j++) {
                if (x_col.getEntry(j)!=0.0){
                    mean += x_col.getEntry(j);
                    nonzero += 1;
                }
            }
            if (nonzero != 0) {
                for (int j = 0; j < x_col.getDimension(); j++) {
                    if (x_col.getEntry(j)!=0.0)
                        x_col.addToEntry(j, -mean/nonzero);
                }

                double sqrt = x_col.getNorm();
                for (int j = 0; j < x_col.getDimension(); j++) {
                    x_col.setEntry(j, x_col.getEntry(j)/sqrt);
                }
            }
            Xcols.setColumnVector(i, x_col);
        }

        double mean_y = 0.0;
        long nonzero_y = 0;
        for(int i = 0; i < Ycol.getDimension(); i++){
            if (Ycol.getEntry(i)!=0.0) {
                mean_y += Ycol.getEntry(i);
                nonzero_y += 1;
            }
        }
        if (nonzero_y != 0){
            for (int i = 0; i < Ycol.getDimension(); i++) {
                if (Ycol.getEntry(i) != 0.0){
                    Ycol.addToEntry(i, -mean_y/nonzero_y);
                }
            }
            double sqrt = Ycol.getNorm();
            for (int i = 0; i < Ycol.getDimension(); i++) {
                Ycol.setEntry(i, Ycol.getEntry(i)/sqrt);
            }
        }
    }


}
class solveLasso implements Runnable{
    private int localThreadId;
    private LassoProblem lassoProblem;
    private static Logger log = LogManager.getLogger(Lasso_ps.class.getName());
    public solveLasso(int localThreadId, LassoProblem lassoProblem){
        this.localThreadId = localThreadId;this.lassoProblem = lassoProblem;
    }
    @Override
    public void run() {
        TableGroup.RegisterThread();
        TableFloat betaTable = TableGroup.GetTableOrDieFloat(0);
        int totalNumWorkers = lassoProblem.tableGroupConfig.getNum_total_clients()
                * (lassoProblem.tableGroupConfig.getNum_local_app_threads()-1);
        int globalWorkerId = lassoProblem.tableGroupConfig.getClient_id()
                * (lassoProblem.tableGroupConfig.getNum_local_app_threads()-1)
                + localThreadId;
        RowAccessor betaAcc = new RowAccessor();
        betaTable.Get(0, betaAcc);
        DenseFloatRow betaVal = betaAcc.GetDenseFloatRow();
        // Run additional iterations to let stale values finish propagating
        for (int i = 0; i < lassoProblem.Xcols.getColumnDimension(); i++) {
            betaTable.Inc(0, i, - betaVal.get(i));
        }
        lassoProblem.Residual = lassoProblem.Ycol.copy();
        for (int i = 0;
             i < lassoProblem.clientTableConfig.getTable_info().getTable_staleness();
             i++) {
            TableGroup.Clock();
        }
        log.info("Start lasso");
        // Run lasso solver
        for (int iter = 0; iter < lassoProblem.lassoConfig.numIters; iter++){
            long time_begin = System.currentTimeMillis();
            // Output loss function
            if (globalWorkerId == 0){
                log.info("Iteration "+(iter+1)+" ...");
                double obj = 0;
                betaTable.Get(0, betaAcc);
                betaVal = betaAcc.GetDenseFloatRow();
                obj = Math.pow(lassoProblem.Residual.getNorm(),2);
                obj /= 2;
                for (int col_i = 0; col_i < lassoProblem.lassoConfig.numCols; col_i++) {
                    obj += lassoProblem.lassoConfig.lambda * Math.abs(betaVal.get(col_i));
                }
                log.info("loss function = "+obj+" ...");
            }
            // Divide beta coefficients elements across workers, and perform coordinate descent
            for (int a = globalWorkerId; a < lassoProblem.lassoConfig.numCols;
                 a += totalNumWorkers){
                lassoElement(a, iter, betaTable);
            }
            // Advance Parameter Server iteration
            TableGroup.Clock();
            long time_end = System.currentTimeMillis();
            if (globalWorkerId == 0){
                log.info("elapsed time = "+(time_end-time_begin)/1000.0);
            }
        }
        // Run additional iterations to let stale values finish propagating
        for (int i = 0;
             i < lassoProblem.clientTableConfig.getTable_info().getTable_staleness(); i++) {
            TableGroup.Clock();
        }
        // Output data
        if(globalWorkerId == 0){
            outputResult(lassoProblem.lassoConfig.outfile);
        }
        // Deregister this thread with Petuum PS
        TableGroup.DeregisterThread();
    }

    private void lassoElement(int a, int iter, TableFloat betaTable) {
        // get beta value
        RowAccessor beta_acc = new RowAccessor();
        betaTable.Get(0, beta_acc);
        DenseFloatRow beta_val = beta_acc.GetDenseFloatRow();

        float new_beta = 0.0f;

        // Compute value for updating beta(a)
        float val_before_thr = 0.0f;
        RealVector Xcol = lassoProblem.Xcols.getColumnVector(a);
        val_before_thr = (float) Xcol.dotProduct(lassoProblem.Residual);
        val_before_thr += beta_val.get(a);
        // soft thresdholing
        if(val_before_thr>0 && lassoProblem.lassoConfig.lambda<Math.abs(val_before_thr)){
            new_beta = (float) (val_before_thr - lassoProblem.lassoConfig.lambda);
        }else if (val_before_thr<0 && lassoProblem.lassoConfig.lambda<Math.abs(val_before_thr)){
            new_beta = (float) (val_before_thr + lassoProblem.lassoConfig.lambda);
        }else
            new_beta = 0;
        // Update residual
        lassoProblem.Residual.combineToSelf(1,beta_val.get(a) - new_beta, Xcol);
        // Commit updates to Petuum PS
        betaTable.Inc(0, a, new_beta - beta_val.get(a));
    }

    public void outputResult(String output_file) {
        TableFloat betaTable = TableGroup.GetTableOrDieFloat(0);
        RowAccessor betaAcc = new RowAccessor();
        betaTable.Get(0, betaAcc);
        DenseFloatRow betaVal = betaAcc.GetDenseFloatRow();
        try {
            File out_file = new File(output_file);
            if(!out_file.getParentFile().exists())
                out_file.getParentFile().mkdirs();
            FileWriter fw = new FileWriter(output_file);
            for (int i = 0; i < lassoProblem.lassoConfig.numCols; i++) {
                fw.write(betaVal.get(i) + "\n");
            }
            fw.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
