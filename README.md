Petuum v1.1
===========

To install Petuum, please refer to the Petuum wiki manual: https://github.com/petuum/bosen/wiki<br>

Website: http://petuum.org

For support, or to report bugs, please send email to petuum-user@googlegroups.com. Please provide your name and affiliation when requesting support; we do not support anonymous inquiries.

Overview
========

Petuum is a distributed machine learning framework. It takes care of the difficult system "plumbing work", allowing you to focus on the ML. Petuum runs efficiently at scale on research clusters and cloud compute like Amazon EC2 and Google GCE.

Petuum provides essential distributed programming tools to tackle the challenges of running ML at scale: Big Data (many data samples) and Big Models (very large parameter and intermediate variable spaces). Unlike general-purpose distributed programming platforms, Petuum is designed specifically for ML algorithms. This means that Petuum takes advantage of data correlation, staleness, and other statistical properties to maximize the performance for ML algorithms, realized through core features such as Bösen, a bounded-asynchronous key-value store, and Strads, a scheduler for iterative ML computations.

In addition to distributed ML programming tools, Petuum comes with many distributed ML algorithms, all implemented on top of the Petuum framework for speed and scalability. Please refer to the Petuum wiki for a full listing: https://github.com/petuum/bosen/wiki

Petuum comes from "perpetuum mobile," which is a musical style characterized by a continuous steady stream of notes. Paganini's Moto Perpetuo is an excellent example. It is our goal to build a system that runs efficiently and reliably -- in perpetual motion.

This repository contains the Bösen parameter server of Petuum. More details can be found in our paper published in [SoCC'15](http://dl.acm.org/citation.cfm?id=2806778&CFID=637243775&CFTOKEN=54057576) section 3.1 and 3.2. 
