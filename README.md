Recent Changes
==============

Version 0.93:<br>
-More compatibility fixes for Lasso/LR; should now work on a wider range of configurations<br>
-Hotfix 11/20/2014: Fixed an initialization issue with the PS, that affected LDA under some configurations<br>

Version 0.92:<br>
-Fixed Lasso/LR not working on some machine configurations<br>
-Added --ps_row_cache_size option to MF, to control memory usage on very large matrices<br>
-Fixed LDA not saving log-likelihood to disk<br>

Version 0.91:<br>
-Fixed minor bugs in the LDA app<br>

Version 0.9:<br>
-Initial Petuum beta release<br>

Petuum
==============

Wiki manual (latest version of Petuum only): http://github.com/sailinglab/petuum/wiki
PDF manual (for this version of Petuum): https://github.com/petuum/public/raw/release_0.93/Petuumv0.93documentation.pdf

Website: http://www.petuum.org.

Petuum is a distributed machine learning framework. It takes care of the difficult system "plumbing work", allowing you to focus on the ML. Petuum runs efficiently at scale on research clusters and cloud compute like Amazon EC2 and Google GCE.

Petuum provides essential distributed programming tools to tackle the challenges of running ML at scale: Big Data (many data samples) and Big Models (very large parameter and intermediate variable spaces). Petuum addresses these challenges with a distributed parameter server (key-value storage), a distributed model scheduler, and out-of-core (disk) storage. Unlike general-purpose distributed programming platforms, Petuum is designed specifically for ML algorithms. This means that Petuum takes advantage of data correlation, staleness, and other statistical properties to maximize the performance for ML algorithms.

In addition to distributed ML programming tools, Petuum comes with several distributed ML algorithms, all implemented on top of the Petuum framework for speed and scalability:<br>
-Latent Dirichlet Allocation (a.k.a Topic Modeling)<br>
-Least-squares Matrix Factorization (a.k.a Collaborative Filtering)<br>
-Lasso and Logistic regression<br>
-Deep Neural Network<br>

Petuum comes from "perpetuum mobile," which is a musical style characterized by a continuous steady stream of notes. Paganini's Moto Perpetuo is an excellent example. It is our goal to build a system that runs efficiently and reliably -- in perpetual motion.
