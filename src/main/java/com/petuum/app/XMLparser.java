package com.petuum.app;

import com.petuum.petuum_ps.*;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import org.dom4j.Attribute;
import org.dom4j.Document;
import org.dom4j.Element;
import org.dom4j.io.SAXReader;

import java.io.File;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Created by bobo on 14-6-11.
 */
public class XMLparser {
    /**
     * Parse the xml file of configuration, and modify the contents of tableGroupConfig and ClientTableConfig
     * provided.
     * @param XMLConfigFileName
     * @param table_group_config
     * @param client_table_config_map  The list to contain the client table configs
     */
    public static void parseTableConfigs(String XMLConfigFileName, com.petuum.petuum_ps.TableGroupConfig table_group_config,
                             Map<Integer, ClientTableConfig> client_table_config_map){
        try {
            client_table_config_map.clear();
            SAXReader saxReader = new SAXReader();
            Document document = saxReader.read(new File(XMLConfigFileName));
            Element docEle = document.getRootElement();
            Element elem_table_group = docEle.element("tablegroup");
            if(elem_table_group != null ) {

                /** table number **/
                table_group_config.setNum_tables(
                        Integer.parseInt(elem_table_group.attributeValue("num_tables")));
                /** thread number **/
                table_group_config.setNum_total_server_threads(
                        Integer.parseInt(elem_table_group.attributeValue("num_total_server_threads")));
                table_group_config.setNum_total_bg_threads(
                        Integer.parseInt(elem_table_group.attributeValue("num_total_bg_threads")));
                table_group_config.setNum_total_clients(
                        Integer.parseInt(elem_table_group.attributeValue("num_total_clients")));
                table_group_config.setNum_local_server_threads(
                        Integer.parseInt(elem_table_group.attributeValue("num_local_server_threads")));
                table_group_config.setNum_local_app_threads(
                        Integer.parseInt(elem_table_group.attributeValue("num_local_app_threads")));
                table_group_config.setNum_local_bg_threads(
                        Integer.parseInt(elem_table_group.attributeValue("num_local_bg_threads")));
                /** ssp model **/
                if (elem_table_group.attributeValue("consistency_model").equals("SSP")) {
                    table_group_config.setConsistency_model(ConsistencyModel.SSP);
                } else if (elem_table_group.attributeValue("consistency_model").equals("SSPPush")) {
                    table_group_config.setConsistency_model(ConsistencyModel.SSPPush);
                } else if (elem_table_group.attributeValue("consistency_model").equals("SSPPushValueBound")) {
                    table_group_config.setConsistency_model(ConsistencyModel.SSPPushValueBound);
                }
            }
            List<Element> elementList1 = docEle.element("clienttables").elements();
            if(elementList1 != null) {
                for (Element subElem : elementList1) {      //each table
                    ClientTableConfig client_table_config = new ClientTableConfig();
                    client_table_config.getTable_info().setRow_type(
                            Integer.parseInt(subElem.attributeValue("row_type")));
                    client_table_config.getTable_info().setRow_capacity(
                            Integer.parseInt(subElem.attributeValue("row_capacity")));
                    client_table_config.getTable_info().setTable_staleness(
                            Integer.parseInt(subElem.attributeValue("table_staleness")));
                    client_table_config.setOplog_capacity(
                            Integer.parseInt(subElem.attributeValue("oplog_capacity")));
                    client_table_config.setProcess_cache_capacity(
                            Integer.parseInt(subElem.attributeValue("process_cache_capacity")));
                    client_table_config.setThread_cache_capacity(
                            Integer.parseInt(subElem.attributeValue("thread_cache_capacity")));
                    client_table_config_map.put(Integer.parseInt(subElem.attributeValue("id")),
                            client_table_config);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    public static void parseParam(String XMLParamFileName, Map<String, String> paramMap){
        try {
            paramMap.clear();
            SAXReader saxReader = new SAXReader();
            Document document = saxReader.read(new File(XMLParamFileName));
            Element docEle = document.getRootElement();
            if (docEle != null&&docEle.getName().equals("parameters")) {
                List<Attribute> alst=docEle.attributes();
                if (alst != null)
                    for (Attribute elem: alst){
                        paramMap.put(elem.getName(), elem.getValue());
                    }
            }
        }catch(Exception e){
            e.printStackTrace();
        }
    }
    //for test
    private static Logger logger = LogManager.getLogger(XMLparser.class.getName());
    public static void main(String args[]){

        TableGroupConfig tableGroupConfig = new TableGroupConfig();
        Map<Integer, ClientTableConfig> clientTableConfigMap = new HashMap<Integer, ClientTableConfig>();
        XMLparser.parseTableConfigs("config/table_HelloWorld.xml", tableGroupConfig, clientTableConfigMap);
        for (int i = 0; i < tableGroupConfig.getHost_map().size(); i++) {
            HostInfo hi = tableGroupConfig.getHost_map().get(i);
            System.out.println("machine: " + hi.getIp() + " : " + hi.getPort());
        }
        Map<String, String> paramMap=new HashMap<String, String>();
        XMLparser.parseParam("config/param_Lasso.xml", paramMap);
        System.out.println(paramMap.get("num_rows"));
        logger.info("XMLparse!");
    }
}
