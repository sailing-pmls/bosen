/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package cn.edu.buaa.act.petuumOnYarn;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Vector;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.GnuParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.io.IOUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.classification.InterfaceAudience;
import org.apache.hadoop.classification.InterfaceStability;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.permission.FsPermission;
import org.apache.hadoop.io.DataOutputBuffer;
import org.apache.hadoop.security.Credentials;
import org.apache.hadoop.security.UserGroupInformation;
import org.apache.hadoop.security.token.Token;
import org.apache.hadoop.yarn.api.ApplicationConstants;
import org.apache.hadoop.yarn.api.ApplicationConstants.Environment;
import org.apache.hadoop.yarn.api.protocolrecords.GetNewApplicationResponse;
import org.apache.hadoop.yarn.api.records.ApplicationId;
import org.apache.hadoop.yarn.api.records.ApplicationReport;
import org.apache.hadoop.yarn.api.records.ApplicationSubmissionContext;
import org.apache.hadoop.yarn.api.records.ContainerLaunchContext;
import org.apache.hadoop.yarn.api.records.FinalApplicationStatus;
import org.apache.hadoop.yarn.api.records.LocalResource;
import org.apache.hadoop.yarn.api.records.LocalResourceType;
import org.apache.hadoop.yarn.api.records.LocalResourceVisibility;
import org.apache.hadoop.yarn.api.records.NodeReport;
import org.apache.hadoop.yarn.api.records.NodeState;
import org.apache.hadoop.yarn.api.records.Priority;
import org.apache.hadoop.yarn.api.records.QueueACL;
import org.apache.hadoop.yarn.api.records.QueueInfo;
import org.apache.hadoop.yarn.api.records.QueueUserACLInfo;
import org.apache.hadoop.yarn.api.records.Resource;
import org.apache.hadoop.yarn.api.records.YarnApplicationState;
import org.apache.hadoop.yarn.api.records.YarnClusterMetrics;
import org.apache.hadoop.yarn.client.api.YarnClient;
import org.apache.hadoop.yarn.client.api.YarnClientApplication;
import org.apache.hadoop.yarn.conf.YarnConfiguration;
import org.apache.hadoop.yarn.exceptions.YarnException;
import org.apache.hadoop.yarn.util.ConverterUtils;

/**
 @author Haoyue Wang {wanghy11@act.buaa.edu.cn}
 */
@InterfaceAudience.Public
@InterfaceStability.Unstable
public class Client {
	
	private static long startTime;
	private static long currentTime;

	private static final Log LOG = LogFactory.getLog(Client.class);
	
	// Configuration
	private Configuration conf;
	private YarnClient yarnClient;
	// Application master specific info to register a new Application with
	// RM/ASM
	private String appName = "";
	// App master priority
	private int amPriority = 10;
	// Queue for App master
	private String amQueue = "";
	// Amt. of memory resource to request for to run the App Master
	private int amMemory = 500;
	// Amt. of virtual core resource to request for to run the App Master
	private int amVCores = 1;
	private int numNodes = 1;
	private int startPort = 9999;

	// Application master jar file
	private String appMasterJar = "";
	// Main class to invoke application master
	private final String appMasterMainClass;

	// Location of script file
	private String scriptPath = "";
	private String scriptHDFSPath = "";
	// Env variables to be setup for the shell command
	private Map<String, String> shellEnv = new HashMap<String, String>();
	// Shell Command Container priority
	private int workerPriority = 10;

	// Amt of memory to request for container in which script will be
	// executed
	private int containerMemory = 1000;
	// Amt. of virtual cores to request for container in which script will
	// be executed
	private int containerVirtualCores = 2;

	// log4j.properties file
	// if available, add to local resources and set into classpath
	private String log4jPropFile = "";

	// Start time for client
	private final long clientStartTime = System.currentTimeMillis();

	// flag to indicate whether to keep containers across application attempts.
	private boolean keepContainers = false;

	// Debug flag
	boolean debugFlag = false;

	// Command line options
	private Options opts;
	
	private String petuumHDFSPathPrefix = "petuum/";

	private static final String appMasterJarPath = "AppMaster.jar";
	// Hardcoded path to custom log_properties
	private static final String log4jPath = "log4j.properties";	
	private static final String launchPath = "launch.py";	
	
	

	/**
	 * @param args
	 *            Command line arguments
	 */
	public static void main(String[] args) {
		startTime = System.currentTimeMillis();
		boolean result = false;
		try {
			Client client = new Client();
			LOG.info("Initializing Client");
			try {
				boolean doRun = client.init(args);
				if (!doRun) {
					System.exit(0);
				}
			} catch (IllegalArgumentException e) {
				System.err.println(e.getLocalizedMessage());
				client.printUsage();
				System.exit(-1);
			}
			result = client.run();
		} catch (Throwable t) {
			LOG.fatal("Error running Client", t);
			System.exit(1);
		}
		if (result) {
			LOG.info("Application completed successfully");
			System.exit(0);
		}
		LOG.error("Application failed to complete successfully");
		System.exit(2);
	}

	/**
   */
	public Client(Configuration conf) throws Exception {
		this("cn.edu.buaa.act.petuumOnYarn.ApplicationMaster", conf);
	}

	Client(String appMasterMainClass, Configuration conf) {
		this.conf = conf;
		this.appMasterMainClass = appMasterMainClass;
		yarnClient = YarnClient.createYarnClient();
		yarnClient.init(conf);
		opts = new Options();
		opts.addOption("hdfs_path_prefix", true, "petuum dir path prefix on HDFS. default /petuum/");
		opts.addOption("start_port", true, "Start port of each machine");
		opts.addOption("num_nodes", true, "Required number of nodes");
		opts.addOption("app_name", true,
				"Application Name. Default value - Petuum");
		opts.addOption("priority", true, "Application Priority. Default 10");
		opts.addOption("queue", true,
				"RM Queue in which this application is to be submitted.");
		opts.addOption("master_memory", true,
				"Amount of memory in MB to be requested to run the application master. Default 500");
		opts.addOption("master_vcores", true,
				"Amount of virtual cores to be requested to run the application master. Default 1");
		opts.addOption("jar", true,
				"Jar file containing the application master");
		opts.addOption(
				"launch_script_path",
				true,
				"User's launch script path");
		opts.addOption("shell_env", true,
				"Environment for script. Specified as env_key=env_val pairs");
		opts.addOption("worker_priority", true,
				"Priority for the worker containers. Default 10");
		opts.addOption("container_memory", true,
				"Amount of memory in MB to be requested to run the worker. Default 1000");
		opts.addOption("container_vcores", true,
				"Amount of virtual cores to be requested to run the worker. Default 2");
		opts.addOption("log_properties", true, "log4j.properties file");
		opts.addOption(
				"keep_containers_across_application_attempts",
				false,
				"Flag to indicate whether to keep containers across application attempts."
						+ " If the flag is true, running containers will not be killed when"
						+ " application attempt fails and these containers will be retrieved by"
						+ " the new application attempt ");
		opts.addOption(
				"attempt_failures_validity_interval",
				true,
				"when attempt_failures_validity_interval in milliseconds is set to > 0,"
						+ "the failure number will not take failures which happen out of "
						+ "the validityInterval into failure count. "
						+ "If failure count reaches to maxAppAttempts, "
						+ "the application will be failed.");
		opts.addOption("debug", false, "Dump out debug information");
		opts.addOption("view_acls", true, "Users and groups that allowed to "
				+ "view the timeline entities in the given domain");
		opts.addOption("modify_acls", true, "Users and groups that allowed to "
				+ "modify the timeline entities in the given domain");
		opts.addOption("create", false,
				"Flag to indicate whether to create the "
						+ "domain specified with -domain.");
		opts.addOption("help", false, "Print usage");
	}

	/**
   */
	public Client() throws Exception {
		this(new YarnConfiguration());
	}

	/**
	 * Helper function to print out usage
	 */
	private void printUsage() {
		new HelpFormatter().printHelp("Client", opts);
	}

	/**
	 * Parse command line options
	 * 
	 * @param args
	 *            Parsed command line options
	 * @return Whether the init was successful to run the client
	 * @throws ParseException
	 */
	public boolean init(String[] args) throws ParseException {

		CommandLine cliParser = new GnuParser().parse(opts, args);

		if (args.length == 0) {
			throw new IllegalArgumentException(
					"No args specified for client to initialize");
		}

		if (cliParser.hasOption("log_properties")) {
			String log4jPath = cliParser.getOptionValue("log_properties");
			try {
				Log4jPropertyHelper.updateLog4jConfiguration(Client.class,
						log4jPath);
			} catch (Exception e) {
				LOG.warn("Can not set up custom log4j properties. " + e);
			}
		}

		if (cliParser.hasOption("help")) {
			printUsage();
			return false;
		}

		if (cliParser.hasOption("debug")) {
			debugFlag = true;

		}

		if (cliParser.hasOption("keep_containers_across_application_attempts")) {
			LOG.info("keep_containers_across_application_attempts");
			keepContainers = true;
		}

		appName = cliParser.getOptionValue("app_name", "Petuum");
		startPort = Integer
				.parseInt(cliParser.getOptionValue("start_port", "9999"));
		amPriority = Integer
				.parseInt(cliParser.getOptionValue("priority", "10"));
		amQueue = cliParser.getOptionValue("queue", "default");
		amMemory = Integer.parseInt(cliParser.getOptionValue("master_memory",
				"500"));
		amVCores = Integer.parseInt(cliParser.getOptionValue("master_vcores",
				"1"));
		numNodes = Integer.parseInt(cliParser.getOptionValue("num_nodes",
				"1"));
		petuumHDFSPathPrefix = cliParser.getOptionValue("hdfs_path_prefix", "petuum/");

		if (amMemory < 0) {
			throw new IllegalArgumentException(
					"Invalid memory specified for application master, exiting."
							+ " Specified memory=" + amMemory);
		}
		if (amVCores < 0) {
			throw new IllegalArgumentException(
					"Invalid virtual cores specified for application master, exiting."
							+ " Specified virtual cores=" + amVCores);
		}

		if (!cliParser.hasOption("jar")) {
			throw new IllegalArgumentException(
					"No jar file specified for application master");
		}

		appMasterJar = cliParser.getOptionValue("jar");

		
		scriptPath = cliParser.getOptionValue("launch_script_path");
		
		if (cliParser.hasOption("shell_env")) {
			String envs[] = cliParser.getOptionValues("shell_env");
			for (String env : envs) {
				env = env.trim();
				int index = env.indexOf('=');
				if (index == -1) {
					shellEnv.put(env, "");
					continue;
				}
				String key = env.substring(0, index);
				String val = "";
				if (index < (env.length() - 1)) {
					val = env.substring(index + 1);
				}
				shellEnv.put(key, val);
			}
		}
		workerPriority = Integer.parseInt(cliParser.getOptionValue(
				"worker_priority", "10"));

		containerMemory = Integer.parseInt(cliParser.getOptionValue(
				"container_memory", "1000"));
		containerVirtualCores = Integer.parseInt(cliParser.getOptionValue(
				"container_vcores", "2"));
		if (containerMemory < 0 || containerVirtualCores < 0) {
			throw new IllegalArgumentException(
					"Invalid container memory/vcores specified,"
							+ " exiting." + " Specified containerMemory="
							+ containerMemory + ", containerVirtualCores="
							+ containerVirtualCores);
		}

		Long.parseLong(cliParser
				.getOptionValue("attempt_failures_validity_interval", "-1"));

		log4jPropFile = cliParser.getOptionValue("log_properties", "");

		return true;
	}

	/**
	 * Main run function for the client
	 * 
	 * @return true if application completed successfully
	 * @throws IOException
	 * @throws YarnException
	 */
	public boolean run() throws IOException, YarnException {

		LOG.info("Running Client");
		yarnClient.start();
		String[] s;
		s = conf.getStrings(YarnConfiguration.RM_ADDRESS);
		for (String ss : s)
			LOG.info("RM address: " + ss);
		YarnClusterMetrics clusterMetrics = yarnClient.getYarnClusterMetrics();
		LOG.info("Got Cluster metric info from ASM" + ", numNodeManagers="
				+ clusterMetrics.getNumNodeManagers());

		List<NodeReport> clusterNodeReports = yarnClient
				.getNodeReports(NodeState.RUNNING);
		LOG.info("Got Cluster node info from ASM");
		for (NodeReport node : clusterNodeReports) {
			LOG.info("Got node report from ASM for" + ", nodeId="
					+ node.getNodeId() + ", nodeAddress"
					+ node.getHttpAddress() + ", nodeRackName"
					+ node.getRackName() + ", nodeNumContainers"
					+ node.getNumContainers() + ", nodeIdHost"
					+ node.getNodeId().getHost());
		}

		QueueInfo queueInfo = yarnClient.getQueueInfo(this.amQueue);
		LOG.info("Queue info" + ", queueName=" + queueInfo.getQueueName()
				+ ", queueCurrentCapacity=" + queueInfo.getCurrentCapacity()
				+ ", queueMaxCapacity=" + queueInfo.getMaximumCapacity()
				+ ", queueApplicationCount="
				+ queueInfo.getApplications().size()
				+ ", queueChildQueueCount=" + queueInfo.getChildQueues().size());

		List<QueueUserACLInfo> listAclInfo = yarnClient.getQueueAclsInfo();
		for (QueueUserACLInfo aclInfo : listAclInfo) {
			for (QueueACL userAcl : aclInfo.getUserAcls()) {
				LOG.info("User ACL Info for Queue" + ", queueName="
						+ aclInfo.getQueueName() + ", userAcl="
						+ userAcl.name());
			}
		}

		// Get a new application id
		YarnClientApplication app = yarnClient.createApplication();
		GetNewApplicationResponse appResponse = app.getNewApplicationResponse();
		int maxMem = appResponse.getMaximumResourceCapability().getMemory();
		LOG.info("Max mem capabililty of resources in this cluster " + maxMem);

		// A resource ask cannot exceed the max.
		if (amMemory > maxMem) {
			LOG.info("AM memory specified above max threshold of cluster. Using max value."
					+ ", specified=" + amMemory + ", max=" + maxMem);
			amMemory = maxMem;
		}

		int maxVCores = appResponse.getMaximumResourceCapability()
				.getVirtualCores();
		LOG.info("Max virtual cores capabililty of resources in this cluster "
				+ maxVCores);

		if (amVCores > maxVCores) {
			LOG.info("AM virtual cores specified above max threshold of cluster. "
					+ "Using max value."
					+ ", specified="
					+ amVCores
					+ ", max="
					+ maxVCores);
			amVCores = maxVCores;
		}

		// set the application name
		ApplicationSubmissionContext appContext = app
				.getApplicationSubmissionContext();
		ApplicationId appId = appContext.getApplicationId();

		appContext.setKeepContainersAcrossApplicationAttempts(keepContainers);
		appContext.setApplicationName(appName);

		// set local resources for the application master
		// local files or archives as needed
		// In this scenario, the jar file for the application master is part of
		// the local resources
		Map<String, LocalResource> localResources = new HashMap<String, LocalResource>();

		LOG.info("Copy App Master jar from local filesystem and add to local environment");
		// Copy the application master jar to the filesystem
		// Create a local resource to point to the destination jar path
		FileSystem fs = FileSystem.get(conf);
		YarnUtil.copyAndAddToLocalResources(fs, appMasterJar, petuumHDFSPathPrefix, appMasterJarPath, localResources, null);		
		scriptHDFSPath = YarnUtil.copyToHDFS(fs, scriptPath, petuumHDFSPathPrefix, launchPath, null);
        // Set the log4j properties if needed
        if (!log4jPropFile.isEmpty()) {
        	YarnUtil.copyAndAddToLocalResources(fs, log4jPropFile, petuumHDFSPathPrefix, log4jPath, localResources, null);
        }

		// Set the env variables to be setup in the env where the application
		// master will be run
		LOG.info("Set the environment for the application master");
		Map<String, String> env = new HashMap<String, String>();

		// Add AppMaster.jar location to classpath
		// At some point we should not be required to add
		// the hadoop specific classpaths to the env.
		// It should be provided out of the box.
		// For now setting all required classpaths including
		// the classpath to "." for the application jar
		StringBuilder classPathEnv = new StringBuilder(
				Environment.CLASSPATH.$$()).append(
				ApplicationConstants.CLASS_PATH_SEPARATOR).append("./*");
		for (String c : conf
				.getStrings(
						YarnConfiguration.YARN_APPLICATION_CLASSPATH,
						YarnConfiguration.DEFAULT_YARN_CROSS_PLATFORM_APPLICATION_CLASSPATH)) {
			classPathEnv.append(ApplicationConstants.CLASS_PATH_SEPARATOR);
			classPathEnv.append(c.trim());
		}
		classPathEnv.append(ApplicationConstants.CLASS_PATH_SEPARATOR).append(
				"./log4j.properties");

		// add the runtime classpath needed for tests to work
		if (conf.getBoolean(YarnConfiguration.IS_MINI_YARN_CLUSTER, false)) {
			classPathEnv.append(':');
			classPathEnv.append(System.getProperty("java.class.path"));
		}

		env.put("CLASSPATH", classPathEnv.toString());

		// Set the necessary command to execute the application master
		Vector<CharSequence> vargs = new Vector<CharSequence>(30);

		// Set java executable command
		LOG.info("Setting up app master command");
		vargs.add(Environment.JAVA_HOME.$$() + "/bin/java");
		// Set Xmx based on am memory size
		vargs.add("-Xmx" + amMemory + "m");
		// Set class name
		vargs.add(appMasterMainClass);
		// Set params for Application Master
		vargs.add("--container_memory " + String.valueOf(containerMemory));
		vargs.add("--container_vcores " + String.valueOf(containerVirtualCores));
		vargs.add("--num_nodes " + String.valueOf(numNodes));
		vargs.add("--start_port " + String.valueOf(startPort));
		vargs.add("--priority " + String.valueOf(workerPriority));
		vargs.add("--script_hdfs_path " + scriptHDFSPath);

		for (Map.Entry<String, String> entry : shellEnv.entrySet()) {
			vargs.add("--shell_env " + entry.getKey() + "=" + entry.getValue());
		}
		if (debugFlag) {
			vargs.add("--debug");
		}

		vargs.add("1>" + ApplicationConstants.LOG_DIR_EXPANSION_VAR
				+ "/AppMaster.stdout");
		vargs.add("2>" + ApplicationConstants.LOG_DIR_EXPANSION_VAR
				+ "/AppMaster.stderr");

		// Get final commmand
		StringBuilder command = new StringBuilder();
		for (CharSequence str : vargs) {
			command.append(str).append(" ");
		}

		LOG.info("Completed setting up app master command "
				+ command.toString());
		List<String> commands = new ArrayList<String>();
		commands.add(command.toString());

		// Set up the container launch context for the application master
		ContainerLaunchContext amContainer = ContainerLaunchContext
				.newInstance(localResources, env, commands, null, null, null);

		// Set up resource type requirements
		// For now, both memory and vcores are supported, so we set memory and
		// vcores requirements
		Resource capability = Resource.newInstance(amMemory, amVCores);
		appContext.setResource(capability);

		// Service data is a binary blob that can be passed to the application
		// Not needed in this scenario
		// amContainer.setServiceData(serviceData);

		// Setup security tokens
		if (UserGroupInformation.isSecurityEnabled()) {
			// Note: Credentials class is marked as LimitedPrivate for HDFS and
			// MapReduce
			Credentials credentials = new Credentials();
			String tokenRenewer = conf.get(YarnConfiguration.RM_PRINCIPAL);
			if (tokenRenewer == null || tokenRenewer.length() == 0) {
				throw new IOException(
						"Can't get Master Kerberos principal for the RM to use as renewer");
			}

			// For now, only getting tokens for the default file-system.
			final Token<?> tokens[] = fs.addDelegationTokens(tokenRenewer,
					credentials);
			if (tokens != null) {
				for (Token<?> token : tokens) {
					LOG.info("Got dt for " + fs.getUri() + "; " + token);
				}
			}
			DataOutputBuffer dob = new DataOutputBuffer();
			credentials.writeTokenStorageToStream(dob);
			ByteBuffer fsTokens = ByteBuffer.wrap(dob.getData(), 0,
					dob.getLength());
			amContainer.setTokens(fsTokens);
		}

		appContext.setAMContainerSpec(amContainer);

		// Set the priority for the application master
		Priority pri = Priority.newInstance(amPriority);
		appContext.setPriority(pri);

		// Set the queue to which this application is to be submitted in the RM
		appContext.setQueue(amQueue);

		// Submit the application to the applications manager
		// SubmitApplicationResponse submitResp =
		// applicationsManager.submitApplication(appRequest);
		// Ignore the response as either a valid response object is returned on
		// success
		// or an exception thrown to denote some form of a failure
		LOG.info("Submitting application to ASM");

		yarnClient.submitApplication(appContext);

		// Monitor the application
		currentTime = System.currentTimeMillis();
		LOG.info("submit AM in " + (currentTime - startTime) + "ms");
		return monitorApplication(appId);
	}

	/**
	 * Monitor the submitted application for completion. Kill application if
	 * time expires.
	 * 
	 * @param appId
	 *            Application Id of application to be monitored
	 * @return true if application completed successfully
	 * @throws YarnException
	 * @throws IOException
	 */
	private boolean monitorApplication(ApplicationId appId)
			throws YarnException, IOException {

		while (true) {

			// Check app status every 1 second.
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				LOG.debug("Thread sleep in monitoring loop interrupted");
			}

			// Get application report for the appId we are interested in
			ApplicationReport report = yarnClient.getApplicationReport(appId);

			LOG.info("Got application report from ASM for" + ", appId="
					+ appId.getId() + ", clientToAMToken="
					+ report.getClientToAMToken() + ", appDiagnostics="
					+ report.getDiagnostics() + ", appMasterHost="
					+ report.getHost() + ", appQueue=" + report.getQueue()
					+ ", appMasterRpcPort=" + report.getRpcPort()
					+ ", appStartTime=" + report.getStartTime()
					+ ", yarnAppState="
					+ report.getYarnApplicationState().toString()
					+ ", distributedFinalState="
					+ report.getFinalApplicationStatus().toString()
					+ ", appTrackingUrl=" + report.getTrackingUrl()
					+ ", appUser=" + report.getUser());

			YarnApplicationState state = report.getYarnApplicationState();
			FinalApplicationStatus dsStatus = report
					.getFinalApplicationStatus();
			if (YarnApplicationState.FINISHED == state) {
				if (FinalApplicationStatus.SUCCEEDED == dsStatus) {
					LOG.info("Application has completed successfully. Breaking monitoring loop");
					return true;
				} else {
					LOG.info("Application finished unsuccessfully."
							+ " YarnState=" + state.toString()
							+ ", FinalStatus=" + dsStatus.toString()
							+ ". Breaking monitoring loop");
					return false;
				}
			} else if (YarnApplicationState.KILLED == state
					|| YarnApplicationState.FAILED == state) {
				LOG.info("Application did not finish." + " YarnState="
						+ state.toString() + ", FinalStatus="
						+ dsStatus.toString() + ". Breaking monitoring loop");
				return false;
			}
		}

	}
}
