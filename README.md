Parallel ML System (PMLS) v1.1
==============================

To install PMLS, please refer to the PMLS wiki manual: http://docs.petuum.com/en/latest/<br>

Website: http://petuum.com/opensource

For support, or to report bugs, please send email to pmls-support@googlegroups.com. Please provide your name and affiliation when requesting support; we do not support anonymous inquiries.

Overview
========

Parallel ML System (PMLS) is a distributed machine learning framework. It takes care of the difficult system "plumbing work", allowing you to focus on the ML. PMLS runs efficiently at scale on research clusters and cloud compute like Amazon EC2 and Google GCE.

PMLS provides essential distributed programming tools to tackle the challenges of running ML at scale: Big Data (many data samples) and Big Models (very large parameter and intermediate variable spaces). Unlike general-purpose distributed programming platforms, PMLS is designed specifically for ML algorithms. This means that PMLS takes advantage of data correlation, staleness, and other statistical properties to maximize the performance for ML algorithms, realized through core features such as Bösen, a bounded-asynchronous key-value store, and Strads, a scheduler for iterative ML computations.

In addition to distributed ML programming tools, PMLS comes with many distributed ML algorithms, all implemented on top of the PMLS framework for speed and scalability. Please refer to the PMLS documentation for a full listing: http://docs.petuum.com/en/latest

This repository contains the Bösen parameter server of PMLS. More details can be found in section 3.1 and 3.2 of our paper published in [SoCC'15](http://dl.acm.org/citation.cfm?id=2806778&CFID=637243775&CFTOKEN=54057576).
